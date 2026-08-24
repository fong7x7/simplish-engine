#!/bin/bash
# ─────────────────────────────────────────────────────────────────────────────
# test.sh — Run Simplish tests with folder/filter/checklist support
#
# Usage:
#   ./scripts/test.sh                          # Run all tests
#   ./scripts/test.sh engine/core              # Run tests matching "engine.*core"
#   ./scripts/test.sh editor/project           # Run tests matching "editor.*project"
#   ./scripts/test.sh Toolbar                  # Run tests matching "Toolbar"
#   ./scripts/test.sh --checklist              # Run all, write failure checklist
#   ./scripts/test.sh engine/ --checklist      # Scoped checklist
#   ./scripts/test.sh --build                  # Build before running
#   ./scripts/test.sh --coverage               # Generate coverage report
#   ./scripts/test.sh --debug|--release        # Select build type
#   ./scripts/test.sh --changed-only           # Skip when no C++ file changed (git only)
#   ./scripts/test.sh --retry 3                # Re-run failures up to N times
# ─────────────────────────────────────────────────────────────────────────────
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_DIR="$(cd "${SCRIPT_DIR}/.." && pwd)"
cd "${PROJECT_DIR}"

# ── Defaults ────────────────────────────────────────────────────────────────

BUILD_TYPE="debug"
DO_BUILD=false
DO_COVERAGE=false
DO_CHANGED_ONLY=false
RETRY_COUNT=0
DO_CHECKLIST=true
CHECKLIST_FILE="test-failures.md"
FILTERS=()
TEST_EXIT=0
TEST_OUTPUT_FILE=""

# ── Usage ───────────────────────────────────────────────────────────────────

show_usage() {
    cat <<'USAGE'
Usage: scripts/test.sh [OPTIONS] [FILTER...]

Run Simplish tests. Optionally filter by folder, module, or test name.

Filters:
  engine/core           Run tests whose name matches "engine.*core"
  editor/project        Run tests matching "editor.*project"
  Toolbar               Run tests matching "Toolbar"
  Multiple filters      Combined with OR (any match runs)

CTest test names come from Catch2 TEST_CASE titles, so filters match the
prose in the test name, not the source path.

Options:
  --debug               Use debug build (default)
  --release             Use release build
  --headless            Use the headless (stub RHI) build
  --preset NAME         Use an arbitrary CMake preset
  --build               Build before running tests
  --coverage            Generate coverage report after tests
  --changed-only        Exit early when no C++ file has changed (needs git)
  --retry N             Re-run failing tests up to N times (default: no retry)
  --no-checklist        Do not write a markdown checklist
  --checklist [FILE]    Checklist output path (default: test-failures.md)
  --checklist=FILE      Same
  -h, --help            Show this help

Examples:
  scripts/test.sh editor/project --build
  scripts/test.sh engine/gui
  scripts/test.sh --no-checklist Toolbar
  scripts/test.sh viewport camera
USAGE
}

# ── Argument parsing ────────────────────────────────────────────────────────

while [ $# -gt 0 ]; do
    case "$1" in
        --release)    BUILD_TYPE="release"; shift ;;
        --debug)      BUILD_TYPE="debug"; shift ;;
        --headless)   BUILD_TYPE="headless"; shift ;;
        --preset)
            if [ $# -ge 2 ]; then
                BUILD_TYPE="$2"; shift 2
            else
                echo "--preset requires a preset name" >&2; exit 1
            fi
            ;;
        --preset=*)   BUILD_TYPE="${1#--preset=}"; shift ;;
        --build)      DO_BUILD=true; shift ;;
        --coverage)   DO_COVERAGE=true; shift ;;
        --changed-only) DO_CHANGED_ONLY=true; shift ;;
        --retry)
            if [ $# -ge 2 ] && [[ "$2" =~ ^[0-9]+$ ]]; then
                RETRY_COUNT="$2"; shift 2
            else
                echo "--retry requires a number" >&2; exit 1
            fi
            ;;
        --retry=*)
            RETRY_COUNT="${1#--retry=}"
            if ! [[ "${RETRY_COUNT}" =~ ^[0-9]+$ ]]; then
                echo "--retry requires a number" >&2; exit 1
            fi
            shift
            ;;
        --no-checklist)
            DO_CHECKLIST=false; shift ;;
        --checklist=*)
            DO_CHECKLIST=true
            CHECKLIST_FILE="${1#--checklist=}"
            [ -z "${CHECKLIST_FILE}" ] && CHECKLIST_FILE="test-failures.md"
            shift
            ;;
        --checklist)
            DO_CHECKLIST=true
            if [ $# -ge 2 ] && [[ "$2" != -* ]] && { [[ "$2" == *.md ]] || [[ "$2" == *.txt ]]; }; then
                CHECKLIST_FILE="$2"
                shift
            fi
            shift
            ;;
        -h|--help)    show_usage; exit 0 ;;
        -*)           echo "Unknown option: $1"; show_usage; exit 1 ;;
        *)            FILTERS+=("$1"); shift ;;
    esac
done

if [ -z "${CHECKLIST_FILE}" ]; then
    CHECKLIST_FILE="test-failures.md"
fi
if [[ "${CHECKLIST_FILE}" != /* ]]; then
    CHECKLIST_FILE="${PROJECT_DIR}/${CHECKLIST_FILE}"
fi

BUILD_DIR="build/${BUILD_TYPE}"

# ── Resolve filters to a concrete set of test names ────────────────────────

# A filter matches either a test's package label (set by simplish_add_test —
# "editor/project", "engine/gui") or its Catch2 test-case title. Catch2 names
# tests by their prose title, which says nothing about where the code lives,
# so matching labels is what makes path-style filters work at all.
#
# Both are resolved here rather than handed to `ctest -R`, because -R matches
# names only and -R plus -L is an AND, not the OR we want.

build_ctest_filter() {
    if [ "${#FILTERS[@]}" -eq 0 ]; then
        return
    fi
    local parts=()
    local f pattern
    for f in "${FILTERS[@]}"; do
        f="${f%/}"                      # strip trailing slash
        pattern="${f//\//.*}"            # engine/core -> engine.*core
        parts+=("${pattern}")
    done
    local IFS='|'
    echo "${parts[*]}"
}

CTEST_FILTER=$(build_ctest_filter)

# Expand a name/label filter into an anchored alternation of exact test names.
# Prints the regex on stdout; prints nothing when nothing matched.
resolve_filter_to_names() {
    local build_dir="$1"
    local pattern="$2"
    ctest --test-dir "${build_dir}" --show-only=json-v1 2>/dev/null \
        | python3 -c '
import json, re, sys

pattern = sys.argv[1]
try:
    data = json.load(sys.stdin)
except ValueError:
    sys.exit(0)

rx = re.compile(pattern, re.IGNORECASE)
matched = []
for test in data.get("tests", []):
    name = test.get("name", "")
    labels = []
    for prop in test.get("properties", []):
        if prop.get("name") == "LABELS":
            labels = prop.get("value", [])
    haystacks = [name] + list(labels)
    if any(rx.search(h) for h in haystacks):
        matched.append(name)

if matched:
    print("|".join("^" + re.escape(n) + "$" for n in matched))
' "${pattern}"
}

# ── Build (optional) ───────────────────────────────────────────────────────

if [ "${DO_BUILD}" = true ]; then
    echo "=== Building (${BUILD_TYPE}) ==="
    cmake --preset "${BUILD_TYPE}"
    cmake --build "${BUILD_DIR}"
fi

# ── Verify build dir exists ────────────────────────────────────────────────

if [ ! -d "${BUILD_DIR}" ]; then
    echo "Error: Build directory '${BUILD_DIR}' not found."
    echo "Run with --build or build manually first: cmake --preset ${BUILD_TYPE} && cmake --build ${BUILD_DIR}"
    exit 1
fi

# ── Skip tests when no C++ files changed (opt-in) ──────────────────────────

# Only under --changed-only. As a default this is dangerous: a clean tree makes
# the script print success and exit 0 without running a single test, which is
# indistinguishable from a passing run for anything reading the exit code.
#
# Every git call is guarded. `set -o pipefail` is in force, so an unguarded
# `x=$(git ... | head -1)` in a non-git directory propagates git's exit 128
# through the pipeline and kills the script under `set -e` — silently, before
# any output. That is what this block used to do here.
if [ "${DO_CHANGED_ONLY}" = true ] && [ -z "${CTEST_FILTER}" ]; then
    if ! git rev-parse --is-inside-work-tree >/dev/null 2>&1; then
        echo "=== --changed-only ignored: not a git repository ==="
    elif ! git rev-parse --verify HEAD >/dev/null 2>&1; then
        # No commits yet: there is no baseline to diff against, so "nothing
        # changed" would be a false negative. Run everything.
        echo "=== --changed-only ignored: no commits yet ==="
    else
        cpp_changed=""
        for range in "HEAD" "--cached"; do
            if [ -z "${cpp_changed}" ]; then
                cpp_changed=$(git diff --name-only "${range}" -- \
                    '*.cpp' '*.h' '*.hpp' '*.mm' 2>/dev/null | head -1 || true)
            fi
        done
        if [ -z "${cpp_changed}" ] && git rev-parse --verify main >/dev/null 2>&1; then
            merge_base=$(git merge-base HEAD main 2>/dev/null || true)
            if [ -n "${merge_base}" ]; then
                cpp_changed=$(git diff --name-only "${merge_base}" HEAD -- \
                    '*.cpp' '*.h' '*.hpp' '*.mm' 2>/dev/null | head -1 || true)
            fi
        fi
        if [ -z "${cpp_changed}" ]; then
            echo "=== No C++ files changed — skipping tests ==="
            exit 0
        fi
    fi
fi

# ── Run tests ──────────────────────────────────────────────────────────────

PARALLEL_JOBS=$(sysctl -n hw.ncpu 2>/dev/null || nproc 2>/dev/null || echo 4)
# Over-subscribe logical cores by 2x — tests are mostly short and often
# blocked on process spawn rather than CPU, so a higher -j cuts wall time
# without saturating the machine.
OVERSUB_JOBS=$(( PARALLEL_JOBS * 2 ))

CTEST_ARGS=(
    --test-dir "${BUILD_DIR}"
    --output-on-failure
    -j "${OVERSUB_JOBS}"
    # Per-test timeout so a single hanging test cannot block the full run.
    --timeout 120
)

# Retry is opt-in. Automatic re-runs turn a flaky test into a passing one and
# hide the flake, so the default is a single attempt and a real failure.
if [ "${RETRY_COUNT}" -gt 0 ]; then
    CTEST_ARGS+=(--repeat "until-pass:${RETRY_COUNT}")
fi

if [ -n "${CTEST_FILTER}" ]; then
    RESOLVED_FILTER=$(resolve_filter_to_names "${BUILD_DIR}" "${CTEST_FILTER}" || true)
    if [ -z "${RESOLVED_FILTER}" ]; then
        echo "No tests match: ${CTEST_FILTER}" >&2
        echo "" >&2
        echo "Available packages:" >&2
        ctest --test-dir "${BUILD_DIR}" --show-only=json-v1 2>/dev/null \
            | python3 -c '
import json, sys
data = json.load(sys.stdin)
seen = {}
for test in data.get("tests", []):
    for prop in test.get("properties", []):
        if prop.get("name") == "LABELS":
            for label in prop.get("value", []):
                seen[label] = seen.get(label, 0) + 1
for label in sorted(seen):
    print(f"  {label} ({seen[label]} tests)")
' >&2
        exit 1
    fi
    CTEST_ARGS+=(-R "${RESOLVED_FILTER}")
    echo "=== Running tests matching: ${CTEST_FILTER} ==="
else
    echo "=== Running all tests ==="
fi

if [ "${DO_CHECKLIST}" = true ]; then
    # Capture output for checklist generation
    TEST_OUTPUT_FILE=$(mktemp)
    trap 'rm -f "${TEST_OUTPUT_FILE:-}"' EXIT
    set +e
    ctest "${CTEST_ARGS[@]}" 2>&1 | tee "${TEST_OUTPUT_FILE}"
    TEST_EXIT=${PIPESTATUS[0]}
    set -e
else
    set +e
    ctest "${CTEST_ARGS[@]}"
    TEST_EXIT=$?
    set -e
fi

# ── Checklist generation ───────────────────────────────────────────────────

generate_checklist() {
    local output_file="$1"
    local checklist="$2"
    local filter="$3"

    # Parsing is done in Python rather than sed. CTest reports a failure as
    #   1/34 Test #96: <name> ...***Failed    0.01 sec
    # and Catch2 test names are prose sentences containing spaces, commas and
    # regex metacharacters. The previous sed pipeline captured `[^ ]+`, which
    # truncated every name at its first space — and the truncated name then
    # failed to match when slicing out that test's output, so the failure
    # detail block always came out empty.
    #
    # Prints the failure count on stdout.
    python3 "${SCRIPT_DIR}/lib/ctest_checklist.py" \
        "${output_file}" "${checklist}" "${BUILD_TYPE}" "${filter}"
}

if [ "${DO_CHECKLIST}" = true ]; then
    mkdir -p "$(dirname "${CHECKLIST_FILE}")"
    FAIL_COUNT=$(generate_checklist "${TEST_OUTPUT_FILE}" "${CHECKLIST_FILE}" \
        "${CTEST_FILTER}" || echo "?")
    echo ""
    if [ "${FAIL_COUNT}" = "0" ]; then
        echo "PASS: All tests passed. Checklist: ${CHECKLIST_FILE}"
    else
        echo "FAIL: ${FAIL_COUNT} test failure(s). Checklist: ${CHECKLIST_FILE}"
    fi
    printf '%s\n' "${CHECKLIST_FILE}"
fi

# ── Coverage report ────────────────────────────────────────────────────────

if [ "${DO_COVERAGE}" = true ]; then
    # Find test executables registered with CTest (respecting any active filter)
    CTEST_SHOW_ARGS=(--test-dir "${BUILD_DIR}" --show-only=json-v1)
    if [ -n "${CTEST_FILTER}" ]; then
        CTEST_SHOW_ARGS+=(-R "${CTEST_FILTER}")
    fi

    TEST_BINARIES=$(ctest "${CTEST_SHOW_ARGS[@]}" 2>/dev/null \
        | python3 -c "
import sys, json
data = json.load(sys.stdin)
seen = set()
for t in data.get('tests', []):
    cmd = t.get('command', [])
    if cmd:
        exe = cmd[0]
        if exe not in seen:
            seen.add(exe)
            print(exe)
" 2>/dev/null)

    if [ -z "${TEST_BINARIES}" ]; then
        echo "Error: No test binaries found. Did you build with tests enabled?"
        exit 1
    fi

    # Build llvm-profdata / llvm-cov object args
    PROFDATA="${BUILD_DIR}/coverage.profdata"
    OBJECT_ARGS=()
    FIRST_BINARY=""
    while IFS= read -r bin; do
        [ -z "${bin}" ] && continue
        if [ -z "${FIRST_BINARY}" ]; then
            FIRST_BINARY="${bin}"
        else
            OBJECT_ARGS+=("-object=${bin}")
        fi
    done <<< "${TEST_BINARIES}"

    # Merge raw profiles into an array (safe for paths with spaces)
    PROFRAW_FILES=()
    while IFS= read -r -d '' pfile; do
        PROFRAW_FILES+=("${pfile}")
    done < <(find "${BUILD_DIR}" -name "*.profraw" -print0 2>/dev/null)

    if [ "${#PROFRAW_FILES[@]}" -eq 0 ]; then
        echo ""
        echo "No .profraw files found in ${BUILD_DIR}."
        echo "Rebuild with coverage instrumentation:"
        echo "  cmake --preset ${BUILD_TYPE} -DCMAKE_C_FLAGS='--coverage -fprofile-instr-generate -fcoverage-mapping' \\"
        echo "        -DCMAKE_CXX_FLAGS='--coverage -fprofile-instr-generate -fcoverage-mapping'"
        exit 1
    fi

    # xcrun resolves the toolchain on macOS; elsewhere the LLVM tools are
    # expected on PATH.
    if command -v xcrun >/dev/null 2>&1; then
        LLVM_PROFDATA=(xcrun llvm-profdata)
        LLVM_COV=(xcrun llvm-cov)
    elif command -v llvm-profdata >/dev/null 2>&1; then
        LLVM_PROFDATA=(llvm-profdata)
        LLVM_COV=(llvm-cov)
    else
        echo "Error: llvm-profdata / llvm-cov not found on PATH." >&2
        exit 1
    fi

    "${LLVM_PROFDATA[@]}" merge -sparse "${PROFRAW_FILES[@]}" -o "${PROFDATA}"

    # Print summary
    echo ""
    echo "=== Coverage Report ==="
    "${LLVM_COV[@]}" report "${FIRST_BINARY}" "${OBJECT_ARGS[@]+"${OBJECT_ARGS[@]}"}" \
        -instr-profile="${PROFDATA}" \
        -ignore-filename-regex='(/test/|_deps/|build/)'

    echo ""
    echo "For detailed HTML report run:"
    echo "  ${LLVM_COV[*]} show \"${FIRST_BINARY}\" ${OBJECT_ARGS[*]+\"${OBJECT_ARGS[*]}\"} -instr-profile=\"${PROFDATA}\" -format=html -output-dir=coverage_html -ignore-filename-regex='(/test/|_deps/|build/)'"
fi

exit ${TEST_EXIT}
