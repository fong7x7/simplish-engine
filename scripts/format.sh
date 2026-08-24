#!/usr/bin/env bash

set -e

# Get the directory where this script lives
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_DIR="$(cd ${SCRIPT_DIR}/.. && pwd)"

# On macOS, Homebrew LLVM is keg-only — add it to PATH if available
if [ "$(uname -s)" = "Darwin" ] && ! command -v clang-format &>/dev/null; then
    LLVM_PREFIX="$(brew --prefix llvm 2>/dev/null)"
    if [ -n "${LLVM_PREFIX}" ] && [ -x "${LLVM_PREFIX}/bin/clang-format" ]; then
        export PATH="${LLVM_PREFIX}/bin:${PATH}"
    fi
fi

# Detect CPU core count for parallel formatting
if [ "$(uname -s)" = "Darwin" ]; then
    JOBS=$(sysctl -n hw.ncpu 2>/dev/null || echo 4)
else
    JOBS=$(nproc 2>/dev/null || echo 4)
fi

# All buildable code lives under src/, one package per directory
# (docs/development/code-layout.md). Tests live inside their package's test/
# folder, so they are picked up by the same walk — there is no separate
# tests/ root to list.
SOURCE_DIRS=(src)
FIND_PRUNE='\( -name build -o -name _deps -o -name third_party -o -name CMakeFiles \) -type d -prune'
FIND_MATCH='-type f \( -name "*.h" -o -name "*.cpp" -o -name "*.mm" \) -print0'

FILES=()
for dir in "${SOURCE_DIRS[@]}"; do
    [ -d "${PROJECT_DIR}/${dir}" ] || continue
    while IFS= read -r -d '' file; do
        FILES+=("$file")
    done < <(eval find "\"${PROJECT_DIR}/${dir}\"" "${FIND_PRUNE}" -o "${FIND_MATCH}" 2>/dev/null)
done

TOTAL=${#FILES[@]}
if [ "${TOTAL}" -eq 0 ]; then
    echo "No .h / .cpp / .mm files to format."
    exit 0
fi

printf '%s\0' "${FILES[@]}" | xargs -0 -P "${JOBS}" -n 20 clang-format -i
printf '  format: 100%% (%d/%d files)\n' "${TOTAL}" "${TOTAL}" >&2
