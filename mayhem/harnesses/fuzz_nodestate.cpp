// Fuzzes the saved-state blobs of Element's built-in nodes (the per-node "state" stored inside a
// session) and the MIDI they then process. Input layout:
//   byte 0         which NodeFactory node (audio router, MIDI channel splitter, MIDI monitor,
//                  MIDI program map, MIDI router, script node)
//   bytes 1..2     little-endian length L of the state blob
//   next L bytes   the blob, given to Processor::setState as a session load does
//   the rest       MIDI events for one render block: [sample offset][length n][n raw bytes] ...
// The node is then prepared, renders one block, and getState is round-tripped through setState.
// OSC sender/receiver (open UDP sockets) and the MCU node (opens MIDI devices) are left out.
#include "fuzz_common.hpp"
#include <element/nodefactory.hpp>
#include <element/processor.hpp>
#include "nodes/nodetypes.hpp"

using namespace element;
using namespace juce;

namespace {
NodeFactory* factory = nullptr;
const char* const ids[] = {
    EL_NODE_ID_AUDIO_ROUTER,
    EL_NODE_ID_MIDI_CHANNEL_SPLITTER,
    EL_NODE_ID_MIDI_MONITOR,
    EL_NODE_ID_MIDI_PROGRAM_MAP,
    EL_NODE_ID_MIDI_ROUTER,
    EL_NODE_ID_SCRIPT,
};
constexpr int blockSize = 256;
} // namespace

extern "C" int LLVMFuzzerInitialize (int*, char***)
{
    elfuzz::initJuce();
    factory = new NodeFactory();
    return 0;
}

extern "C" int LLVMFuzzerTestOneInput (const uint8_t* data, size_t size)
{
    if (factory == nullptr)
        LLVMFuzzerInitialize (nullptr, nullptr);
    if (size < 3)
        return -1;
    const char* id = ids[data[0] % (sizeof (ids) / sizeof (ids[0]))];
    size_t stateLen = (size_t) data[1] | ((size_t) data[2] << 8);
    data += 3;
    size -= 3;
    stateLen = std::min (stateLen, size);
    const uint8_t* state = data;
    data += stateLen;
    size -= stateLen;

    ProcessorPtr node = factory->instantiate (String (id));
    if (node == nullptr)
        return 0;
    // As GraphNode::addNode does: ports are set up before the node is configured and prepared.
    node->refreshPorts();
    node->setState (state, (int) stateLen);
    node->refreshPorts();
    node->prepareToRender (44100.0, blockSize);

    const int nAudio = jmax (1, node->getNumAudioInputs(), node->getNumAudioOutputs());
    const int nMidi = jmax (1, node->getNumPorts (PortType::Midi, true), node->getNumPorts (PortType::Midi, false));
    AudioSampleBuffer audio (nAudio, blockSize);
    audio.clear();
    AudioSampleBuffer cv (1, blockSize);
    cv.clear();
    OwnedArray<MidiBuffer> midis;
    Array<int> midiIdx;
    for (int i = 0; i < nMidi; ++i)
    {
        midis.add (new MidiBuffer());
        midiIdx.add (i);
    }
    // Length-checked MIDI events, the way a MIDI device callback hands raw messages to the engine.
    size_t pos = 0;
    while (pos + 2 <= size)
    {
        const int offset = data[pos] % blockSize;
        const size_t n = data[pos + 1];
        pos += 2;
        if (n == 0 || pos + n > size)
            break;
        midis.getUnchecked (0)->addEvent (data + pos, (int) n, offset);
        pos += n;
    }

    RenderContext rc (audio.getArrayOfWritePointers(), nAudio, cv.getArrayOfWritePointers(), 0, midis, midiIdx, blockSize);
    node->render (rc);

    MemoryBlock saved;
    node->getState (saved);
    node->setState (saved.getData(), (int) saved.getSize());
    node->releaseResources();
    return 0;
}
