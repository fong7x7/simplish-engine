#!/bin/bash
set -euo pipefail

# Aggregate driver: clang-tidy + coding invariants → one checklist by default.
#
# Usage:
#   ./scripts/lint.sh [OPTION ...] [PATH ...]
#
# Default: writes lint-checklist.md (clang-tidy + invariant sections), minimal stdout.
#   --checklist[=FILE]   Consolidated markdown path (default: lint-checklist.md)
#   --no-checklist       Forward args only; full console output from both tools
#   --all                Lint all files (default: only files changed vs main)
#
# Other options are passed through to clang-tidy.sh; invariant checks receive the same
# set minus clang-tidy-only flags (--debug, --release, --fix, --jobs, -jN).

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_DIR="$(cd "${SCRIPT_DIR}/.." && pwd)"
cd "${PROJECT_DIR}"

CHECKLIST_MODE=true
CONSOLIDATED_OUT="lint-checklist.md"
LINT_ALL=false
PASSTHROUGH=()

for arg in "$@"; do
    case "$arg" in
        --no-checklist) CHECKLIST_MODE=false ;;
        --checklist)    CONSOLIDATED_OUT="lint-checklist.md" ;;
        --checklist=*)  CONSOLIDATED_OUT="${arg#--checklist=}" ;;
        --all)          LINT_ALL=true ;;
        *)              PASSTHROUGH+=("${arg}") ;;
    esac
done

if [ -z "${CONSOLIDATED_OUT}" ]; then
    CONSOLIDATED_OUT="lint-checklist.md"
fi

if [[ "${CONSOLIDATED_OUT}" != /* ]]; then
    CONSOLIDATED_OUT="${PROJECT_DIR}/${CONSOLIDATED_OUT}"
fi

# If no explicit paths and not --all, auto-detect changed files vs main
has_paths=false
for arg in ${PASSTHROUGH[@]+"${PASSTHROUGH[@]}"}; do
    case "$arg" in
        -*) ;;
        *) has_paths=true; break ;;
    esac
done

if [ "${has_paths}" = false ] && [ "${LINT_ALL}" = false ]; then
    main_branch="main"
    if git rev-parse --verify "${main_branch}" &>/dev/null; then
        merge_base=$(git merge-base HEAD "${main_branch}" 2>/dev/null || echo "")
        if [ -n "${merge_base}" ]; then
            changed_files=()
            while IFS= read -r f; do
                [[ "$f" == *.h || "$f" == *.cpp ]] && [ -f "$f" ] && changed_files+=("$f")
            done < <(git diff --name-only "${merge_base}" HEAD 2>/dev/null; git diff --name-only 2>/dev/null)
            if [ ${#changed_files[@]} -gt 0 ]; then
                deduped=()
                while IFS= read -r uf; do
                    deduped+=("$uf")
                done < <(printf '%s\n' "${changed_files[@]}" | sort -u)
                PASSTHROUGH+=("${deduped[@]}")
            else
                echo "No changed C++ files vs ${main_branch}. Nothing to lint."
                exit 0
            fi
        fi
    fi
fi

check_invariants_args=()
for arg in ${PASSTHROUGH[@]+"${PASSTHROUGH[@]}"}; do
    case "$arg" in
        --debug|--release|--fix) ;;
        --jobs=*|-j[0-9]*) ;;
        *) check_invariants_args+=("${arg}") ;;
    esac
done

if [ "${CHECKLIST_MODE}" = true ]; then
    ./scripts/format.sh 2>/dev/null
else
    ./scripts/format.sh
fi

if [ "${CHECKLIST_MODE}" != true ]; then
    tidy_rc=0
    ./scripts/clang-tidy.sh ${PASSTHROUGH[@]+"${PASSTHROUGH[@]}"} || tidy_rc=$?

    inv_rc=0
    ./scripts/check-invariants.sh ${check_invariants_args[@]+"${check_invariants_args[@]}"} || inv_rc=$?

    if [ "${tidy_rc}" -ne 0 ] || [ "${inv_rc}" -ne 0 ]; then
        exit 1
    fi
    exit 0
fi

tidy_tmp="$(mktemp "${TMPDIR:-/tmp}/lint-clang-tidy.XXXXXX")"
inv_tmp="$(mktemp "${TMPDIR:-/tmp}/lint-invariants.XXXXXX")".md
mv "${inv_tmp%.md}" "${inv_tmp}"
cleanup() { rm -f "${tidy_tmp:-}" "${inv_tmp:-}"; }
trap cleanup EXIT

./scripts/clang-tidy.sh --quiet --checklist="${tidy_tmp}" ${PASSTHROUGH[@]+"${PASSTHROUGH[@]}"} 2>/dev/null

inv_rc=0
./scripts/check-invariants.sh -q --checklist "${inv_tmp}" ${check_invariants_args[@]+"${check_invariants_args[@]}"} 2>/dev/null || inv_rc=$?

{
    echo "# Lint findings checklist"
    echo ""
    echo "**Generated:** $(date +%Y-%m-%d)"
    echo ""
    echo "---"
    echo ""
    cat "${tidy_tmp}"
    echo ""
    echo "---"
    echo ""
    if [ -s "${inv_tmp}" ]; then
        cat "${inv_tmp}"
    else
        echo "## Coding invariants"
        echo ""
        echo "_No C++ files in scope (invariant checklist not generated)._"
    fi
} > "${CONSOLIDATED_OUT}"

printf '%s\n' "${CONSOLIDATED_OUT}"

tidy_ok=1
if [ -s "${tidy_tmp}" ] && ! grep -qx 'No clang-tidy findings.' "${tidy_tmp}"; then
    tidy_ok=0
fi

if [ "${inv_rc}" -ne 0 ] || [ "${tidy_ok}" -eq 0 ]; then
    exit 1
fi
exit 0
