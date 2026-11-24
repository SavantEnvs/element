// Fuzzes Element's Lua DSP script path: ScriptNode::loadScript compiles the input as a DSP script
// (ScriptLoader + DSPScript: layout/port counts, parameters, controls), then the node is prepared,
// renders one audio+MIDI block through the script's process(), and its state is saved and restored
// (DSPScript::save/restore). Upstream fixes 095b7d8495 (midi port count clamp) and c2bb200a32
// (syntax errors) live on this path. Modelled on test/ScriptNodeTests.cpp.
#include "fuzz_common.hpp"
#include "nodes/scriptnode.hpp"

using namespace element;
using namespace juce;

extern "C" int LLVMFuzzerInitialize (int*, char***)
{
    elfuzz::initJuce();
    return 0;
}

extern "C" int LLVMFuzzerTestOneInput (const uint8_t* data, size_t size)
{
    elfuzz::initJuce();
    const auto code = String::fromUTF8 ((const char*) data, (int) size);

    ScriptNode::Ptr node = new ScriptNode();
    if (node->loadScript (code).failed())
        return 0;

    node->refreshPorts();
    node->prepareToRender (44100.0, 256);
    {
        // Size the buffers from the node's ports, as GraphNode does when it renders a node.
        const int nAudio = jmax (1, node->getNumAudioInputs(), node->getNumAudioOutputs());
        const int nMidi = jmax (1, node->getNumPorts (PortType::Midi, true), node->getNumPorts (PortType::Midi, false));
        AudioSampleBuffer audio (nAudio, 256);
        audio.clear();
        for (int s = 0; s < audio.getNumSamples(); ++s)
            audio.setSample (0, s, std::sin ((float) s * 0.05f));
        AudioSampleBuffer cv (1, 256);
        cv.clear();
        OwnedArray<MidiBuffer> midis;
        Array<int> midiIdx;
        for (int i = 0; i < nMidi; ++i)
        {
            auto* mb = midis.add (new MidiBuffer());
            mb->addEvent (MidiMessage::noteOn (1 + (i % 16), 60, 0.8f), 0);
            mb->addEvent (MidiMessage::controllerEvent (1, 7, 100), 10);
            mb->addEvent (MidiMessage::noteOff (1 + (i % 16), 60), 100);
            midiIdx.add (i);
        }
        RenderContext rc (audio.getArrayOfWritePointers(), nAudio, cv.getArrayOfWritePointers(), 0, midis, midiIdx, 256);
        node->render (rc);
    }

    MemoryBlock state;
    node->getState (state);
    ScriptNode::Ptr other = new ScriptNode();
    other->setState (state.getData(), (int) state.getSize());
    node->releaseResources();
    return 0;
}
