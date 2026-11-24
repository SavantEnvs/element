// Shared setup for the Element fuzz harnesses: JUCE is initialised once per process, the way
// upstream's test/TestMain.cpp does it (MessageManager + initialiseJuce_GUI, no window is opened).
#pragma once
#include <cstddef>
#include <cstdint>
#include <element/juce.hpp>

namespace elfuzz {
inline void initJuce()
{
    static bool done = false;
    if (done)
        return;
    done = true;
    juce::MessageManager::getInstance();
    juce::initialiseJuce_GUI();
}
} // namespace elfuzz
