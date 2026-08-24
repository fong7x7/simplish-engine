#!/bin/bash
# Launch the Simplish Editor. Builds if needed, then runs.
#
# Usage:
#   ./scripts/editor.sh [PROJECT_DIR]      # open a project directory
#   ./scripts/editor.sh                    # open with no project loaded
#   ./scripts/editor.sh --release [ARGS]   # run the release build
#
# Stack trace on crash (no code changes):
#   SIMPLISH_LLDB=1 ./scripts/editor.sh
#     Runs under LLDB; on SIGSEGV/exceptions prints "thread backtrace all".
#   SIMPLISH_LLDB=i ./scripts/editor.sh
#     Interactive LLDB (type "bt" or "thread backtrace all" after it stops).
#
# macOS also writes crash reports under:
#   ~/Library/Logs/DiagnosticReports/simplish-editor-*.ips
set -uo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_DIR="$(cd "${SCRIPT_DIR}/.." && pwd)"

BUILD_TYPE="debug"
if [ $# -gt 0 ] && { [ "$1" = "--release" ] || [ "$1" = "--debug" ]; }; then
    BUILD_TYPE="${1#--}"
    shift
fi

BUILD_DIR="${PROJECT_DIR}/build/${BUILD_TYPE}"
# CMake target name, not the output name — the binary is named simplish-editor.
EDITOR_TARGET="simplish-editor-app"
EDITOR_BIN="${BUILD_DIR}/src/bin/editor/simplish-editor"

cd "${PROJECT_DIR}" || exit 1

# Configure when the build tree has never been configured. Testing for the
# cache rather than the directory matters: a partially-created build/ (from an
# interrupted run, or from `mkdir`) has the directory but no build system.
if [ ! -f "${BUILD_DIR}/CMakeCache.txt" ]; then
    echo "Configuring ${BUILD_TYPE} build..."
    if ! cmake --preset "${BUILD_TYPE}"; then
        echo "Configure failed." >&2
        exit 1
    fi
fi

echo "Building ${EDITOR_TARGET}..."
if ! cmake --build "${BUILD_DIR}" --target "${EDITOR_TARGET}"; then
    echo "Build failed." >&2
    exit 1
fi

if [ ! -x "${EDITOR_BIN}" ]; then
    echo "Editor binary not found at ${EDITOR_BIN}" >&2
    echo "Is ENGINE_BUILD_EDITOR enabled for this preset?" >&2
    exit 1
fi

# Engine init reads ENGINE_DATA_DIR (see src/bin/editor/src/main.cpp). Create
# the directory so a fresh clone launches without a manual mkdir.
export ENGINE_DATA_DIR="${PROJECT_DIR}/data"
mkdir -p "${ENGINE_DATA_DIR}"

case "${SIMPLISH_LLDB:-}" in
    1)
        echo "Launching under LLDB (batch: run -> thread backtrace all -> quit)..."
        exec lldb -b -o run -o "thread backtrace all" -o quit -- "${EDITOR_BIN}" "$@"
        ;;
    i)
        echo "Launching under LLDB (interactive). After a crash: thread backtrace all"
        exec lldb -- "${EDITOR_BIN}" "$@"
        ;;
    *)
        echo "Launching Simplish Editor..."
        exec "${EDITOR_BIN}" "$@"
        ;;
esac
