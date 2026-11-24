// Fuzzes Element's session/graph file loading: the input is written to a scratch file and read the
// way the app opens a .els/.elg file: Session::readFromFile (gzip'd binary ValueTree),
// Session::migrate, Session::loadData (missing properties, controller-map migration, orphan MIDI
// mapping cleanup), a walk over graphs/nodes/ports, createXml; then Node::parse on the same file
// (session -> active graph, else XML, else binary ValueTree) and Node::migrate.
// One Context is kept for the process (built like test/TestMain.cpp, without starting services);
// every input replaces the whole session tree via loadData, so no input state carries over.
#include "fuzz_common.hpp"
#include <element/context.hpp>
#include <element/node.hpp>
#include <element/session.hpp>

#include <cstdlib>
#include <string>
#include <vector>
#include <unistd.h>

using namespace element;
using namespace juce;

namespace {
Context* context = nullptr;
File scratch;

void walk (const Node& graph, int depth)
{
    if (depth > 8)
        return;
    (void) graph.getName();
    (void) graph.getUuid();
    for (int i = 0; i < graph.getNumNodes(); ++i)
    {
        const auto node = graph.getNode (i);
        (void) node.getNumPorts();
        (void) node.getFormat();
        (void) node.getIdentifier();
        if (node.isGraph())
            walk (node, depth + 1);
    }
}
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

    auto session = context->session();
    const auto raw = Session::readFromFile (scratch);
    if (raw.isValid())
    {
        String error;
        const auto migrated = Session::migrate (raw, error);
        if (migrated.isValid() && session->loadData (migrated))
        {
            for (int i = 0; i < session->getNumGraphs(); ++i)
                walk (session->getGraph (i), 0);
            (void) session->createXml();
        }
    }

    const auto nodeData = Node::parse (scratch);
    if (nodeData.isValid())
    {
        String error;
        const auto migrated = Node::migrate (nodeData, error);
        if (migrated.isValid())
            walk (Node (migrated, false), 0);
    }
    return 0;
}
