// Fuzzes graph instantiation from a session file, the way the engine loads an opened .els:
// Session::readFromFile -> Session::migrate -> Session::loadData, then the active graph (a copy of
// its tree) is handed to GraphManager::setNodeModel, which creates every node (restoring each node's
// state blob into a live processor), adds every arc (arbitrary node ids / port indices) and enforces
// the IO nodes. The graph is then rebuilt and renders one audio+MIDI block.
// One Context is kept for the process (as fuzz_session / test/TestMain.cpp, no services started).
// Per input, the GraphNode, GraphManager and model copy are fresh and are cleared/destroyed before
// returning, and loadData replaces the whole session tree, so no input state carries over.
#include "fuzz_common.hpp"
#include <element/context.hpp>
#include <element/node.hpp>
#include <element/plugins.hpp>
#include <element/session.hpp>
#include "engine/graphmanager.hpp"
#include "engine/graphnode.hpp"

#include <cstdlib>
#include <string>
#include <vector>
#include <unistd.h>

using namespace element;
using namespace juce;

namespace {
Context* context = nullptr;
File scratch;
constexpr int blockSize = 256;
} // namespace

extern "C" int LLVMFuzzerInitialize (int*, char***)
{
    elfuzz::initJuce();
    context = new Context (RunMode::Standalone);
    const char* tmp = std::getenv ("TMPDIR");
    std::string templ = std::string (tmp != nullptr && *tmp ? tmp : "/tmp") + "/element-fuzz-XXXXXX";
    std::vector<char> buf (templ.begin(), templ.end());
    buf.push_back (0);
    const int fd = mkstemp (buf.data());
    if (fd >= 0)
        close (fd);
    scratch = File (String::fromUTF8 (buf.data()));
    return 0;
}

extern "C" int LLVMFuzzerTestOneInput (const uint8_t* data, size_t size)
{
    if (context == nullptr)
        LLVMFuzzerInitialize (nullptr, nullptr);
    if (! scratch.replaceWithData (data, size))
        return 0;

    const auto raw = Session::readFromFile (scratch);
    if (! raw.isValid())
        return -1;
    String error;
    const auto migrated = Session::migrate (raw, error);
    auto session = context->session();
    if (! migrated.isValid() || ! session->loadData (migrated))
        return -1;
    const auto active = session->getActiveGraph();
    if (! active.isValid())
        return 0;

    const Node model (active.data().createCopy(), true);
    ProcessorPtr keep (new GraphNode (*context));
    auto& graph = *static_cast<GraphNode*> (keep.get());
    graph.prepareToRender (44100.0, blockSize);
    {
        GraphManager manager (graph, context->plugins());
        manager.setNodeModel (model);
        graph.rebuild();

        const int nAudio = jmax (1, graph.getNumAudioInputs(), graph.getNumAudioOutputs());
        const int nMidi = jmax (1, graph.getNumPorts (PortType::Midi, true), graph.getNumPorts (PortType::Midi, false));
        AudioSampleBuffer audio (nAudio, blockSize);
        audio.clear();
        for (int s = 0; s < blockSize; ++s)
            audio.setSample (0, s, std::sin ((float) s * 0.05f));
        AudioSampleBuffer cv (1, blockSize);
        cv.clear();
        OwnedArray<MidiBuffer> midis;
        Array<int> midiIdx;
        for (int i = 0; i < nMidi; ++i)
        {
            auto* mb = midis.add (new MidiBuffer());
            mb->addEvent (MidiMessage::noteOn (1, 60, 0.8f), 0);
            mb->addEvent (MidiMessage::controllerEvent (1, 7, 100), 10);
            mb->addEvent (MidiMessage::noteOff (1, 60), 100);
            midiIdx.add (i);
        }
        RenderContext rc (audio.getArrayOfWritePointers(), nAudio, cv.getArrayOfWritePointers(), 0, midis, midiIdx, blockSize);
        graph.render (rc);

        manager.savePluginStates();
        manager.clear();
    }
    graph.releaseResources();
    graph.clear();
    return 0;
}
