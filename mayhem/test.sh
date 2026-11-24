#!/usr/bin/env bash
#
# mayhem/test.sh — runs upstream's Boost.Test unit suite (test_element, built by mayhem/build.sh with
# normal Release flags), one CTRF test per suite registered in test/CMakeLists.txt (add_test). Each
# suite asserts values (BOOST_REQUIRE*), and Boost.Test prints "*** No errors detected" only when its
# test cases actually ran and passed, which this script requires.
#
# IMPORTANT:
#  * Must assert BEHAVIOR/OUTPUT, not just exit status. The oracle has to check asserted values /
#    golden-output diffs / known-answer results — so a PATCH that "fixes" a bug by making the program
#    exit(0) (or any no-op) FAILS here. Running inputs and checking only "exit 0 / didn't crash" is
#    NOT a functional test (it's trivially reward-hackable) — use the project's real assertion suite.
#  * Do NOT build here — mayhem/build.sh already compiled the test suite (with the project's normal
#    flags). This script only RUNS the pre-built tests and reports counts. If the test runner is
#    missing, that's a build.sh bug — fail loudly rather than silently rebuilding.
#  * REQUIRED OUTPUT — a CTRF (https://ctrf.io) summary so Mayhem/the PATCH grader reads the counts:
#      - writes a CTRF JSON report to ${CTRF_REPORT:-$SRC/ctrf-report.json}, and
#      - prints a one-line `CTRF {...}` marker to stdout (same JSON, compact).
#    Only `results.summary` (with tests/passed/failed/pending/skipped/other) is required.
#    Use the emit_ctrf helper below; it computes tests = passed+failed+skipped and sets the exit
#    code (0 iff failed==0). Map your framework's output to passed/failed/skipped.
set -uo pipefail
[ -n "${SOURCE_DATE_EPOCH:-}" ] || unset SOURCE_DATE_EPOCH
: "${MAYHEM_JOBS:=$(nproc)}"   # build parallelism; env-overridable, falls back to nproc (use -j"$MAYHEM_JOBS")
cd "$SRC"

# emit_ctrf <tool> <passed> <failed> [skipped] [pending] [other]
# Writes a CTRF report (file + stdout `CTRF {...}` marker) and returns non-zero iff failed>0.
emit_ctrf() {
  local tool="$1" passed="$2" failed="$3" skipped="${4:-0}" pending="${5:-0}" other="${6:-0}"
  local tests=$(( passed + failed + skipped + pending + other ))
  cat > "${CTRF_REPORT:-$SRC/ctrf-report.json}" <<JSON
{
  "results": {
    "tool": { "name": "$tool" },
    "summary": {
      "tests": $tests,
      "passed": $passed,
      "failed": $failed,
      "pending": $pending,
      "skipped": $skipped,
      "other": $other
    }
  }
}
JSON
  printf 'CTRF {"results":{"tool":{"name":"%s"},"summary":{"tests":%d,"passed":%d,"failed":%d,"pending":%d,"skipped":%d,"other":%d}}}\n' \
    "$tool" "$tests" "$passed" "$failed" "$pending" "$skipped" "$other"
  [ "$failed" -eq 0 ]
}


T="$SRC/build-tests/test/test_element_artefacts/Release/test_element"
[ -x "$T" ] || { echo "test_element missing: run mayhem/build.sh first" >&2; exit 1; }
# Suites: every add_test in test/CMakeLists.txt, except MidiOutputTests, which upstream registers
# but no longer defines (Boost reports "no test cases matching filter").
SUITES=$(sed -n 's/^add_test(NAME "\([^"]*\)".*/\1/p' test/CMakeLists.txt | grep -vx MidiOutputTests)
# Tests write settings under $HOME: give them a private one so runs don't depend on each other.
export HOME="$(mktemp -d)"
trap 'rm -rf "$HOME"' EXIT
passed=0; failed=0
for s in $SUITES; do
  out=$(timeout 300 "$T" --run_test="$s" 2>&1); rc=$?
  if [ $rc -eq 0 ] && grep -q "No errors detected" <<<"$out"; then
    passed=$((passed+1))
  else
    failed=$((failed+1)); echo "FAIL: $s (rc=$rc)"; tail -20 <<<"$out"
  fi
done
emit_ctrf "boost-test" "$passed" "$failed" 0
