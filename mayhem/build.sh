#!/usr/bin/env bash
# mayhem/build.sh — builds the sanitized Element static library + fuzz harnesses (build-fuzz/) and
# upstream's Boost.Test suite with normal flags (build-tests/). Offline: JUCE and sol2 come from
# /opt/toolchains/deps (see mayhem/Dockerfile), clap headers from the git submodule.
set -euo pipefail
[ -n "${SOURCE_DATE_EPOCH:-}" ] || unset SOURCE_DATE_EPOCH
: "${SANITIZER_FLAGS=-fsanitize=address,undefined -fno-sanitize-recover=all -fno-omit-frame-pointer}"
: "${DEBUG_FLAGS:=-g -gdwarf-3}"
: "${CC:=clang}" ; : "${CXX:=clang++}" ; : "${LIB_FUZZING_ENGINE:=-fsanitize=fuzzer}"
: "${MAYHEM_JOBS:=$(nproc)}"
: "${COVERAGE_FLAGS=}"
: "${ELEMENT_JUCE_SRC:=/opt/toolchains/deps/juce-8.0.13/src}"
: "${ELEMENT_SOL2_SRC:=/opt/toolchains/deps/sol2-c1f95a773c6f8f4fde8ca3efe872e7286afe4444/src}"
export CC CXX
cd "$SRC"

HARNESSES="fuzz_scriptnode;fuzz_session;fuzz_nodestate;fuzz_gzip;fuzz_graph"

common=(
  -G Ninja
  -DFETCHCONTENT_FULLY_DISCONNECTED=ON
  -DFETCHCONTENT_SOURCE_DIR_JUCE="$ELEMENT_JUCE_SRC"
  -DFETCHCONTENT_SOURCE_DIR_SOL2="$ELEMENT_SOL2_SRC"
  -DCCACHE_PROGRAM=CCACHE_PROGRAM-NOTFOUND
  -DELEMENT_BUILD_PLUGINS=OFF
)

# 1) Fuzz build: the whole library (Element + JUCE modules + Lua) instrumented with ASan/UBSan +
#    SanitizerCoverage and DWARF 3; harness targets are injected via CMAKE_PROJECT_INCLUDE.
SAN_COV=""
[ -n "$SANITIZER_FLAGS" ] && SAN_COV="-fsanitize=fuzzer-no-link"
# UBSan's -fsanitize=function is relaxed: JUCE's bundled LV2 serd/sord (built into every Context)
# calls its C callbacks through mismatched pointer types on startup, which would halt every input.
[ -n "$SANITIZER_FLAGS" ] && SAN_COV="$SAN_COV -fno-sanitize=function"
FLAGS="-O1 -DNDEBUG $SANITIZER_FLAGS $SAN_COV $DEBUG_FLAGS"
cmake -S . -B build-fuzz "${common[@]}" \
  -DCMAKE_BUILD_TYPE= \
  -DELEMENT_BUILD_TESTS=OFF \
  -DCMAKE_C_FLAGS="$FLAGS" -DCMAKE_CXX_FLAGS="$FLAGS" \
  -DCMAKE_PROJECT_INCLUDE="$SRC/mayhem/project_include.cmake" \
  -DELEMENT_FUZZ_HARNESSES="$HARNESSES" \
  -DELEMENT_LSAN_OFF="$SRC/mayhem/lsan_off.cc" \
  -DELEMENT_FUZZ_ENGINE="$LIB_FUZZING_ENGINE" \
  -DELEMENT_STANDALONE_MAIN="$STANDALONE_FUZZ_MAIN"
targets=()
for h in ${HARNESSES//;/ }; do targets+=("$h" "$h-standalone"); done
cmake --build build-fuzz -j"$MAYHEM_JOBS" --target "${targets[@]}"
for h in ${HARNESSES//;/ }; do
  for b in "$h" "$h-standalone"; do
    f=$(find build-fuzz -type f -name "$b" -perm -u+x | head -1)
    [ -n "$f" ] || { echo "missing $b" >&2; exit 1; }
    cp -f "$f" "$SRC/$b"
  done
done

# 2) Upstream's unit tests (Boost.Test, test_element) with the project's normal Release flags.
cmake -S . -B build-tests "${common[@]}" \
  -DCMAKE_BUILD_TYPE=Release \
  -DELEMENT_BUILD_TESTS=ON \
  -DCMAKE_C_FLAGS="$COVERAGE_FLAGS" -DCMAKE_CXX_FLAGS="$COVERAGE_FLAGS"
cmake --build build-tests -j"$MAYHEM_JOBS" --target test_element
