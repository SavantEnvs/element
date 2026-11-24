// Fuzzes element::gzip::decode (base64 -> zlib inflate -> readString), used for the gzip'd strings
// embedded in Element documents. See test/gziptests.cpp.
#include "fuzz_common.hpp"
#include <element/gzip.hpp>

extern "C" int LLVMFuzzerTestOneInput (const uint8_t* data, size_t size)
{
    const auto input = juce::String::fromUTF8 ((const char*) data, (int) size);
    (void) element::gzip::decode (input);
    return 0;
}
