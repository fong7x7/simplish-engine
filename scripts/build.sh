#!/bin/bash
# Configure and build Simplish (CMake preset + build).
#
# Usage:
#   ./scripts/build.sh [--debug|--release|--headless] [--checklist[=FILE]] [--tests] [CMAKE_BUILD_ARGS...]
#
# --tests runs scripts/test.sh with remaining args after a successful build (pass test filters last).
#
# If SIMPLISH_TEST_CHECKLIST is set (absolute or repo-relative path), it is passed as --checklist to test.sh.
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_DIR="$(cd "${SCRIPT_DIR}/.." && pwd)"
cd "${PROJECT_DIR}"

BUILD_TYPE="debug"
CHECKLIST_FILE=""
RUN_TESTS=false
CMAKE_BUILD_EXTRA=()

while [ $# -gt 0 ]; do
    case "$1" in
        --release)  BUILD_TYPE="release"; shift ;;
        --debug)    BUILD_TYPE="debug"; shift ;;
        --headless) BUILD_TYPE="headless"; shift ;;
        --preset)
            if [ $# -ge 2 ]; then
                BUILD_TYPE="$2"; shift 2
            else
                echo "--preset requires a preset name" >&2; exit 1
            fi
            ;;
        --preset=*) BUILD_TYPE="${1#--preset=}"; shift ;;
        -h|--help)
            sed -n '2,10p' "${BASH_SOURCE[0]}" | sed 's|^# \{0,1\}||'
            exit 0
            ;;
        --checklist=*)
            CHECKLIST_FILE="${1#--checklist=}"
            [ -z "${CHECKLIST_FILE}" ] && CHECKLIST_FILE="build-checklist.md"
            shift
            ;;
        --checklist)
            if [ $# -ge 2 ] && [[ "$2" != -* ]] && { [[ "$2" == *.md ]] || [[ "$2" == *.txt ]]; }; then
                CHECKLIST_FILE="$2"
                shift 2
            else
                CHECKLIST_FILE="build-checklist.md"
                shift
            fi
            ;;
        --tests)
            RUN_TESTS=true
            shift
            break
            ;;
        *) CMAKE_BUILD_EXTRA+=("$1"); shift ;;
    esac
done

if [ -n "${CHECKLIST_FILE}" ]; then
    if [[ "${CHECKLIST_FILE}" != /* ]]; then
        CHECKLIST_FILE="${PROJECT_DIR}/${CHECKLIST_FILE}"
    fi
fi

if ! cmake --list-presets 2>/dev/null | grep -q "\"${BUILD_TYPE}\""; then
    echo "Unknown preset: ${BUILD_TYPE}" >&2
    echo "" >&2
    cmake --list-presets >&2 || true
    exit 1
fi

BUILD_DIR="${PROJECT_DIR}/build/${BUILD_TYPE}"

# Build in parallel by default; an explicit -j in CMAKE_BUILD_EXTRA wins
# because it appears later on the command line.
JOBS=$(sysctl -n hw.ncpu 2>/dev/null || nproc 2>/dev/null || echo 4)

run_tests() {
    local tc="${SIMPLISH_TEST_CHECKLIST:-}"
    if [ -n "${tc}" ] && [[ "${tc}" != /* ]]; then
        tc="${PROJECT_DIR}/${tc}"
    fi
    # Pass the preset through: without it, `build.sh --release --tests` built
    # release and then ran the tests in build/debug — whatever happened to be
    # lying there, or nothing at all.
    if [ -n "${tc}" ]; then
        exec "${SCRIPT_DIR}/test.sh" --preset "${BUILD_TYPE}" \
            --checklist="${tc}" "$@"
    fi
    exec "${SCRIPT_DIR}/test.sh" --preset "${BUILD_TYPE}" "$@"
}

write_build_checklist() {
    local out="$1"
    local log="$2"
    local preset="$3"
    local cfg_rc="$4"
    local bld_rc="$5"

    mkdir -p "$(dirname "${out}")"
    {
        echo "# Build checklist"
        echo ""
        echo "**Generated:** $(date -u +%Y-%m-%dT%H:%M:%SZ)"
        echo "**Preset:** ${preset}"
        echo ""
        if [ "${cfg_rc}" -eq 0 ] && [ "${bld_rc}" -eq 0 ]; then
            echo "All phases succeeded."
            echo ""
            echo "| Phase | Status |"
            echo "|-------|--------|"
            echo "| Configure | ok |"
            echo "| Build | ok |"
        else
            echo "## Summary"
            echo ""
            if [ "${cfg_rc}" -ne 0 ]; then
                echo "- [ ] **Configure** (cmake --preset) failed (exit ${cfg_rc})"
            else
                echo "- [x] Configure"
            fi
            if [ "${cfg_rc}" -eq 0 ]; then
                if [ "${bld_rc}" -ne 0 ]; then
                    echo "- [ ] **Build** (cmake --build) failed (exit ${bld_rc})"
                else
                    echo "- [x] Build"
                fi
            else
                echo "- [ ] Build (skipped)"
            fi
            echo ""
            echo "## Log (tail)"
            echo ""
            echo '```'
            tail -n 80 "${log}" 2>/dev/null || echo "(no log)"
            echo '```'
        fi
    } > "${out}"
}

if [ -n "${CHECKLIST_FILE}" ]; then
    LOG_FILE="$(mktemp "${TMPDIR:-/tmp}/simplish-build.XXXXXX")"
    trap 'rm -f "${LOG_FILE:-}"' EXIT

    cfg_rc=0
    cmake --preset "${BUILD_TYPE}" >>"${LOG_FILE}" 2>&1 || cfg_rc=$?

    bld_rc=0
    if [ "${cfg_rc}" -eq 0 ]; then
        cmake --build "${BUILD_DIR}" -j "${JOBS}" \
            "${CMAKE_BUILD_EXTRA[@]+"${CMAKE_BUILD_EXTRA[@]}"}" \
            >>"${LOG_FILE}" 2>&1 || bld_rc=$?
    else
        bld_rc=1
    fi

    write_build_checklist "${CHECKLIST_FILE}" "${LOG_FILE}" "${BUILD_TYPE}" "${cfg_rc}" "${bld_rc}"
    printf '%s\n' "${CHECKLIST_FILE}"

    if [ "${RUN_TESTS}" = true ]; then
        if [ "${cfg_rc}" -ne 0 ] || [ "${bld_rc}" -ne 0 ]; then
            exit 1
        fi
        run_tests "$@"
    fi

    if [ "${cfg_rc}" -ne 0 ] || [ "${bld_rc}" -ne 0 ]; then
        exit 1
    fi
    exit 0
fi

cmake --preset "${BUILD_TYPE}"
cmake --build "${BUILD_DIR}" -j "${JOBS}" \
    "${CMAKE_BUILD_EXTRA[@]+"${CMAKE_BUILD_EXTRA[@]}"}"

if [ "${RUN_TESTS}" = true ]; then
    run_tests "$@"
fi
