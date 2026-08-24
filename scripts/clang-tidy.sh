#!/bin/bash

# Kill all child processes on Ctrl-C so a single interrupt stops everything.
trap 'kill 0 2>/dev/null; exit 130' INT

# Get the directory where this script lives
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

# Navigate to project dir
PROJECT_DIR="$(cd ${SCRIPT_DIR}/.. && pwd)"
cd ${PROJECT_DIR}

# On macOS, Homebrew LLVM is keg-only — add it to PATH if available
if [ "$(uname -s)" = "Darwin" ] && ! command -v clang-tidy &>/dev/null; then
    LLVM_PREFIX="$(brew --prefix llvm 2>/dev/null)"
    if [ -n "${LLVM_PREFIX}" ] && [ -x "${LLVM_PREFIX}/bin/clang-tidy" ]; then
        export PATH="${LLVM_PREFIX}/bin:${PATH}"
    fi
fi

# Detect CPU core count for parallel jobs default
if [ "$(uname -s)" = "Darwin" ]; then
    DEFAULT_JOBS=$(sysctl -n hw.ncpu 2>/dev/null || echo 4)
else
    DEFAULT_JOBS=$(nproc 2>/dev/null || echo 4)
fi

# Parse arguments (default: debug, lint both source and tests; optional PATH… limits scope)
BUILD_TYPE="debug"
FIX=false
LINT_SRC=true
LINT_TESTS=true
CHECKLIST=false
CHECKLIST_FILE=""
QUIET=false
JOBS="${DEFAULT_JOBS}"
TARGET_PATHS=()
for arg in "$@"; do
    case "$arg" in
        --release)    BUILD_TYPE="release" ;;
        --debug)      BUILD_TYPE="debug" ;;
        --fix)        FIX=true ;;
        --src-only)   LINT_SRC=true; LINT_TESTS=false ;;
        --tests-only) LINT_SRC=false; LINT_TESTS=true ;;
        --checklist)  CHECKLIST=true ;;
        --checklist=*) CHECKLIST=true; CHECKLIST_FILE="${arg#--checklist=}" ;;
        --quiet)      QUIET=true ;;
        --jobs=*)     JOBS="${arg#--jobs=}" ;;
        -j[0-9]*)     JOBS="${arg#-j}" ;;
        -*)
            echo "Unknown argument: $arg"
            echo "Usage: $0 [--debug|--release] [--fix] [--src-only|--tests-only] [--checklist[=FILE]] [--quiet] [--jobs=N|-jN] [PATH...]"
            exit 1
            ;;
        *)
            TARGET_PATHS+=("${arg}")
            ;;
    esac
done

BUILD_DIR="build/${BUILD_TYPE}"
COMPILE_DB="${BUILD_DIR}/compile_commands.json"

# compile_commands.json lists .cpp TUs only; clang-tidy on .h files gets no flags unless we
# supply a standard and (on macOS) SDK / libc++ paths. Homebrew clang-tidy also needs the
# same sysroot as Apple Clang so <cstdint>, <filesystem>, etc. resolve like the real build.
# For .cpp files we stop here: the compilation DB already carries all -I flags; piling on
# dozens of extra -isystem paths for _deps was slowing every TU (redundant header search).
# --exclude-header-filter tells clang-tidy to skip analysis entirely on third-party headers,
# not just suppress reporting. This avoids running checks on _deps, build, and system paths.
EXCLUDE_HEADER_FILTER='(build/_deps|third_party|/usr/|/opt/homebrew/|/Library/|catch2)'
CLANG_TIDY_MINIMAL=(--exclude-header-filter="${EXCLUDE_HEADER_FILTER}" --extra-arg-before=-std=c++23)
if [ "$(uname -s)" = "Darwin" ]; then
    SDK_PATH="$(xcrun --show-sdk-path 2>/dev/null)"
    if [ -n "${SDK_PATH}" ] && [ -d "${SDK_PATH}" ]; then
        CLANG_TIDY_MINIMAL+=(
            --extra-arg-before=-isysroot
            --extra-arg-before="${SDK_PATH}"
        )
        CXX_V1="${SDK_PATH}/usr/include/c++/v1"
        if [ -d "${CXX_V1}" ]; then
            CLANG_TIDY_MINIMAL+=(
                --extra-arg-before=-isystem
                --extra-arg-before="${CXX_V1}"
            )
        fi
    fi
    # Homebrew (and other) system libraries install headers to /usr/local/include.
    # CMake IMPORTED targets set INTERFACE_INCLUDE_DIRECTORIES correctly, but
    # compile_commands.json does not always propagate them for OBJECT libraries.
    # Add as -isystem so clang-tidy can resolve headers like lz4.h and zstd.h.
    if [ -d "/usr/local/include" ]; then
        CLANG_TIDY_MINIMAL+=(
            --extra-arg-before=-isystem
            --extra-arg-before=/usr/local/include
        )
    fi
fi

if [ ! -f "${COMPILE_DB}" ]; then
    echo "Error: ${COMPILE_DB} not found. Run build.sh first."
    exit 1
fi

# Header-only TUs: add _deps include roots as -isystem (not used for .cpp — see above).
CLANG_TIDY_DEPS_EXTRAS=()
while IFS= read -r dep_inc; do
    [ -z "${dep_inc}" ] && continue
    [ -d "${dep_inc}" ] || continue
    CLANG_TIDY_DEPS_EXTRAS+=(--extra-arg-before=-isystem --extra-arg-before="${dep_inc}")
done < <(
    {
        grep -oE -- '-I[^ ]+' "${COMPILE_DB}" | sed 's/^-I//' | grep '/_deps/' || true
        grep -oE -- '-isystem[[:space:]]+[^ ]+' "${COMPILE_DB}" | awk '{ print $2 }' | grep '/_deps/' || true
    } | sort -u
)
# Discover all project include directories dynamically so new modules/plugins
# are picked up without editing this script.  Searches all source directories
# for subdirectories named "include".
INCLUDE_SEARCH_DIRS=()
# All packages live under src/ (docs/development/code-layout.md); one root
# is enough, and new packages are picked up without editing this script.
for d in src; do
    [ -d "${PROJECT_DIR}/${d}" ] && INCLUDE_SEARCH_DIRS+=("${PROJECT_DIR}/${d}")
done
# Tests may relax some checks via their own config; fall back to the root
# config when no test-specific one exists (this project has only the root).
if [ -f "${PROJECT_DIR}/src/.clang-tidy" ]; then
    TEST_TIDY_CONFIG="${PROJECT_DIR}/src/.clang-tidy"
else
    TEST_TIDY_CONFIG="${PROJECT_DIR}/.clang-tidy"
fi

PROJECT_INCLUDE_ARGS=()
while IFS= read -r inc_root; do
    PROJECT_INCLUDE_ARGS+=(--extra-arg-before=-I --extra-arg-before="${inc_root}")
done < <(find "${INCLUDE_SEARCH_DIRS[@]}" -name include -type d 2>/dev/null | sort)

# Force C++ mode for .h files (clang defaults to C, which conflicts with -std=c++23).
# Also add project include roots so headers can resolve cross-includes.
CLANG_TIDY_HEADER_EXTRA=(
    --extra-arg=-xc++
    "${CLANG_TIDY_MINIMAL[@]}"
    "${CLANG_TIDY_DEPS_EXTRAS[@]}"
    "${PROJECT_INCLUDE_ARGS[@]}"
)

FIND_PRUNE='\( -name build -o -name _deps -o -name third_party -o -name CMakeFiles \) -type d -prune'
FIND_MATCH='-type f \( -name "*.cpp" -o -name "*.h" \) -print'

# Resolve a user path to an absolute path under PROJECT_DIR (must exist).
resolve_target_path() {
    local raw="$1"
    local out dir base
    if [[ "${raw}" = /* ]]; then
        out="${raw}"
    else
        out="${PROJECT_DIR}/${raw}"
    fi
    dir=$(dirname "${out}")
    base=$(basename "${out}")
    if [ ! -d "${dir}" ]; then
        echo "Error: invalid path: ${raw}"
        return 1
    fi
    dir=$(cd "${dir}" && pwd) || return 1
    echo "${dir}/${base}"
}

# Collect production files and test files separately so each group uses its own .clang-tidy
SRC_CPP_FILES=""
SRC_H_FILES=""
TEST_CPP_FILES=""
TEST_H_FILES=""

if [ "${#TARGET_PATHS[@]}" -eq 0 ]; then
    SRC_FILES=""
    TEST_FILES=""
    if [ "${LINT_SRC}" = true ]; then
        # Tests live inside each package's test/ folder, so the src walk has
        # to exclude them explicitly rather than relying on a separate root.
        SRC_FILES=$(eval find src ${FIND_PRUNE} -o ${FIND_MATCH} 2>/dev/null \
            | grep -v '/test/' | sort)
        [ -n "${SRC_FILES}" ] && SRC_CPP_FILES=$(echo "${SRC_FILES}" | grep '\.cpp$' || true)
        [ -n "${SRC_FILES}" ] && SRC_H_FILES=$(echo "${SRC_FILES}" | grep '\.h$' || true)
    fi
    if [ "${LINT_TESTS}" = true ]; then
        TEST_FILES=$(eval find src ${FIND_PRUNE} -o ${FIND_MATCH} 2>/dev/null \
            | grep '/test/' | sort)
        [ -n "${TEST_FILES}" ] && TEST_CPP_FILES=$(echo "${TEST_FILES}" | grep '\.cpp$' || true)
        [ -n "${TEST_FILES}" ] && TEST_H_FILES=$(echo "${TEST_FILES}" | grep '\.h$' || true)
    fi
else
    MERGED=""
    for raw in "${TARGET_PATHS[@]}"; do
        resolved="$(resolve_target_path "${raw}")" || exit 1
        case "${resolved}" in
            "${PROJECT_DIR}"/*) ;;
            *)
                echo "Error: path must be inside project: ${raw}"
                exit 1
                ;;
        esac
        if [ ! -e "${resolved}" ]; then
            echo "Error: not found: ${raw}"
            exit 1
        fi
        if [ -f "${resolved}" ]; then
            case "${resolved}" in
                *.cpp|*.h) MERGED="${MERGED}${resolved}"$'\n' ;;
                *)
                    echo "Error: not a C++ source or header (.cpp / .h): ${raw}"
                    exit 1
                    ;;
            esac
        elif [ -d "${resolved}" ]; then
            # Same recursive rules as full-tree lint. Must use eval so FIND_PRUNE / FIND_MATCH
            # (parentheses, -name patterns) parse as find primaries — a plain find "$resolved" $vars
            # breaks and yields no files. %q keeps odd paths safe inside eval.
            MERGED="${MERGED}$(eval find "$(printf '%q' "${resolved}")" ${FIND_PRUNE} -o ${FIND_MATCH} 2>/dev/null)"$'\n'
        else
            echo "Error: not a file or directory: ${raw}"
            exit 1
        fi
    done
    MERGED=$(printf '%s' "${MERGED}" | sort -u | sed '/^$/d')
    while IFS= read -r f; do
        [ -z "${f}" ] && continue
        case "${f}" in
            */test/*)
                [ "${LINT_TESTS}" = false ] && continue
                case "${f}" in
                    *.cpp) TEST_CPP_FILES="${TEST_CPP_FILES}${f}"$'\n' ;;
                    *.h) TEST_H_FILES="${TEST_H_FILES}${f}"$'\n' ;;
                esac
                ;;
            *)
                [ "${LINT_SRC}" = false ] && continue
                case "${f}" in
                    *.cpp) SRC_CPP_FILES="${SRC_CPP_FILES}${f}"$'\n' ;;
                    *.h) SRC_H_FILES="${SRC_H_FILES}${f}"$'\n' ;;
                esac
                ;;
        esac
    done <<< "${MERGED}"
fi

SRC_COUNT=0
TEST_COUNT=0
[ -n "${SRC_CPP_FILES}" ] && SRC_COUNT=$((SRC_COUNT + $(echo "${SRC_CPP_FILES}" | grep -c .)))
[ -n "${SRC_H_FILES}" ] && SRC_COUNT=$((SRC_COUNT + $(echo "${SRC_H_FILES}" | grep -c .)))
[ -n "${TEST_CPP_FILES}" ] && TEST_COUNT=$((TEST_COUNT + $(echo "${TEST_CPP_FILES}" | grep -c .)))
[ -n "${TEST_H_FILES}" ] && TEST_COUNT=$((TEST_COUNT + $(echo "${TEST_H_FILES}" | grep -c .)))

TOTAL_COUNT=$((SRC_COUNT + TEST_COUNT))

if [ "${TOTAL_COUNT}" -eq 0 ]; then
    echo "No matching .cpp / .h files to lint."
    if [ "${#TARGET_PATHS[@]}" -gt 0 ]; then
        echo "Hint: paths under a package test/ folder are skipped with --src-only; other paths are skipped with --tests-only."
    fi
    exit 0
fi

# generate_checklist INPUT_FILE PROJECT_DIR OUTPUT_FILE
# Parses raw clang-tidy output and writes a compact markdown checklist.
generate_checklist() {
    local input_file="$1"
    local proj_dir="$2"
    local output_file="$3"

    awk -v project_dir="${proj_dir}/" -v gen_date="$(date +%Y-%m-%d)" -v dash="—" '
    BEGIN {
        total = 0; num_checks = 0; total_files = 0; tu_count = 0
        test_n = 0; engine_n = 0; editor_n = 0; game_n = 0; platform_n = 0
    }

    /^\[[0-9]+\/[0-9]+\] Processing file/ { tu_count++; next }

    {
        line = $0
        n = length(line)
        if (n < 10) next
        if (substr(line, n) != "]") next

        bp = 0
        for (i = n - 1; i > 0; i--) {
            if (substr(line, i, 1) == "[") { bp = i; break }
        }
        if (bp == 0) next

        raw_check = substr(line, bp + 1, n - bp - 1)
        nc = split(raw_check, cp, ",")
        check = cp[nc]

        warn_pos = index(line, ": warning: ")
        err_pos  = index(line, ": error: ")

        if (warn_pos > 0) {
            sev = "warning"; prefix = substr(line, 1, warn_pos - 1)
        } else if (err_pos > 0) {
            sev = "error";   prefix = substr(line, 1, err_pos - 1)
        } else { next }

        np = split(prefix, pp, ":")
        if (np < 3) next
        file = pp[1]
        for (j = 2; j <= np - 2; j++) file = file ":" pp[j]

        sub(project_dir, "", file)
        if (file ~ /^\//) next

        if (!(check in check_total)) {
            check_order[num_checks] = check
            num_checks++
        }
        check_total[check]++
        if (sev == "error") check_sev[check] = "error"
        else if (!(check in check_sev)) check_sev[check] = "warning"

        key = check SUBSEP file
        check_file_count[key]++
        if (!(key in seen_cf)) {
            seen_cf[key] = 1
            fidx = check_nfiles[check] + 0
            check_files[check SUBSEP fidx] = file
            check_nfiles[check] = fidx + 1
        }

        if      (file ~ /^tests\//)    test_n++
        else if (file ~ /^engine\//)   engine_n++
        else if (file ~ /^editor\//)   editor_n++
        else if (file ~ /^game\//)     game_n++
        else if (file ~ /^platform\//) platform_n++

        if (!(file in all_files)) { all_files[file] = 1; total_files++ }
        total++
    }

    function fmt(num,    s, ln, result, k) {
        s = sprintf("%d", num)
        ln = length(s)
        if (ln <= 3) return s
        result = ""
        for (k = ln; k >= 1; k--) {
            result = substr(s, k, 1) result
            if ((ln - k + 1) % 3 == 0 && k > 1) result = "," result
        }
        return result
    }

    END {
        if (total == 0) { print "No clang-tidy findings."; exit 0 }

        # Sort checks by count descending (selection sort)
        for (i = 0; i < num_checks; i++) used[i] = 0
        for (i = 0; i < num_checks; i++) {
            mx = -1; mi = -1
            for (j = 0; j < num_checks; j++) {
                if (!used[j] && check_total[check_order[j]] > mx) {
                    mx = check_total[check_order[j]]; mi = j
                }
            }
            sorted[i] = check_order[mi]; used[mi] = 1
        }

        printf "# Clang-Tidy Findings Checklist\n\n"
        printf "**Generated:** %s\n", gen_date
        printf "**Total findings:** %s across %d files", fmt(total), total_files
        if (tu_count > 0) printf " (%d compilation units analysed)", tu_count
        printf "\n"

        printf "**Distribution:**"
        sep = " "
        if (test_n     > 0) { printf "%stests %s",    sep, fmt(test_n);     sep = " | " }
        if (engine_n   > 0) { printf "%sengine %s",   sep, fmt(engine_n);   sep = " | " }
        if (editor_n   > 0) { printf "%seditor %s",   sep, fmt(editor_n);   sep = " | " }
        if (game_n     > 0) { printf "%sgame %s",     sep, fmt(game_n);     sep = " | " }
        if (platform_n > 0) { printf "%splatform %s", sep, fmt(platform_n); sep = " | " }
        printf "\n\n---\n\n"

        printf "## Summary by Check\n\n"
        printf "| # | Check | Count | Files | Severity |\n"
        printf "|---|-------|------:|------:|----------|\n"
        for (i = 0; i < num_checks; i++) {
            c = sorted[i]
            sv = check_sev[c]
            if (sv == "error") sv = "**error**"
            printf "| %d | %s | %s | %d | %s |\n", i+1, c, fmt(check_total[c]), check_nfiles[c], sv
        }
        printf "\n---\n\n"

        for (i = 0; i < num_checks; i++) {
            c = sorted[i]
            nf = check_nfiles[c]
            printf "## %d. %s (%s findings, %d files)\n\n", i+1, c, fmt(check_total[c]), nf

            # Sort files within this check by count descending
            for (fi = 0; fi < nf; fi++) f_used[fi] = 0
            for (fi = 0; fi < nf; fi++) {
                mxf = -1; mfi = -1
                for (fj = 0; fj < nf; fj++) {
                    if (!f_used[fj]) {
                        fn = check_files[c SUBSEP fj]
                        fc = check_file_count[c SUBSEP fn]
                        if (fc > mxf) { mxf = fc; mfi = fj }
                    }
                }
                f_used[mfi] = 1
                fn = check_files[c SUBSEP mfi]
                fc = check_file_count[c SUBSEP fn]
                printf "- [ ] \x60%s\x60 %s %d\n", fn, dash, fc
            }
            printf "\n---\n\n"
            delete f_used
        }
    }
    ' "${input_file}" > "${output_file}"
}

if [ "${QUIET}" != true ]; then
    if [ "${#TARGET_PATHS[@]}" -gt 0 ]; then
        echo "Running clang-tidy on ${TOTAL_COUNT} file(s) from ${#TARGET_PATHS[@]} path(s) (${SRC_COUNT} non-test, ${TEST_COUNT} under tests/) (build: ${BUILD_TYPE}, jobs: ${JOBS})..."
    else
        echo "Running clang-tidy on ${TOTAL_COUNT} files (${SRC_COUNT} source, ${TEST_COUNT} test) (build type: ${BUILD_TYPE}, jobs: ${JOBS})..."
    fi
    echo ""
fi

FIX_ARG=""
if [ "${FIX}" = true ]; then
    FIX_ARG="--fix"
    if [ "${JOBS}" -gt 1 ] && [ "${QUIET}" != true ]; then
        echo "Note: --fix forces sequential execution (-j1) to avoid parallel writes to shared headers."
        echo ""
    fi
fi

EXIT_CODE=0

# In checklist mode, capture all output to a temp file for post-processing
if [ "${CHECKLIST}" = true ]; then
    TIDY_OUTPUT=$(mktemp "${TMPDIR:-/tmp}/clang-tidy-checklist.XXXXXX")
    PROGRESS_FILE=$(mktemp "${TMPDIR:-/tmp}/clang-tidy-progress.XXXXXX")
    echo 0 > "${PROGRESS_FILE}"
    trap 'rm -f "${TIDY_OUTPUT}" "${PROGRESS_FILE:-}"' EXIT
    [ -z "${CHECKLIST_FILE}" ] && CHECKLIST_FILE="clang-tidy-checklist.md"
fi

# Filter out diagnostic lines from build/_deps and Catch2 macro expansion traces
filter_output() {
    grep --line-buffered -vE "(^${PROJECT_DIR}/build/|INTERNAL_CATCH_|catch2/internal/|CATCH2_INTERNAL_|/catch2/|Use -header-filter=|[0-9]+ warnings? generated|Suppressed [0-9]+ warnings?)"
}

# Run clang-tidy in parallel using xargs -P.
# Usage: run_tidy_parallel FILE_LIST EXTRA_ARGS...
# FILE_LIST is a newline-separated list of files. EXTRA_ARGS are passed to clang-tidy.
# --fix forces -j1 to avoid parallel writes to shared headers.
run_tidy_parallel() {
    local file_list="$1"
    shift
    local tidy_args=("$@")
    local jobs="${JOBS}"
    if [ "${FIX}" = true ]; then
        jobs=1
    fi

    # Convert newline-delimited file list to null-delimited for safe xargs handling.
    # sed strips empty lines (from trailing newlines), tr converts to null-delimited.
    # PIPESTATUS[3] captures xargs exit code (printf=0, sed=1, tr=2, xargs=3, ...).
    # No || true — it resets PIPESTATUS. The script doesn't use set -e, so non-zero
    # pipeline exit is safe; we only need xargs' status for EXIT_CODE.
    #
    # Progress: clang-tidy does not emit [N/M] lines when invoked per-file via xargs,
    # so we write a tiny wrapper script that emits a marker after each file completes.
    # awk counts these markers and prints a running percentage to stderr.
    local PROGRESS_MARKER="@@TIDY_FILE_DONE@@"

    if [ "${CHECKLIST}" = true ]; then
        # Build a wrapper script in a temp file so xargs can invoke it.
        # This avoids fragile sh -c quoting for tidy_args with special chars.
        local wrapper
        wrapper=$(mktemp "${TMPDIR:-/tmp}/clang-tidy-wrap.XXXXXX")
        {
            echo '#!/bin/sh'
            printf 'clang-tidy -p %q' "${BUILD_DIR}"
            for a in "${tidy_args[@]}"; do printf ' %q' "$a"; done
            [ -n "${FIX_ARG}" ] && printf ' %q' "${FIX_ARG}"
            printf ' "$1" 2>&1\n'
            printf 'echo "%s"\n' "${PROGRESS_MARKER}"
        } > "${wrapper}"
        chmod +x "${wrapper}"

        if [ "${QUIET}" = true ]; then
            printf '%s' "${file_list}" | sed '/^$/d' | tr '\n' '\0' \
                | xargs -0 -P "${jobs}" -n 1 "${wrapper}" \
                | filter_output \
                | awk -v total="${TOTAL_COUNT}" -v pf="${PROGRESS_FILE}" -v marker="${PROGRESS_MARKER}" '
                    BEGIN { getline done < pf; close(pf); done += 0 }
                    $0 == marker {
                        done++
                        pct = int(done * 100 / total)
                        printf "\r  clang-tidy: %d%% (%d/%d files)", pct, done, total > "/dev/stderr"
                        fflush("/dev/stderr")
                        next
                    }
                    { print }
                    END { print done > pf; close(pf) }
                ' >> "${TIDY_OUTPUT}"
            EXIT_CODE=$((EXIT_CODE | ${PIPESTATUS[3]}))
        else
            printf '%s' "${file_list}" | sed '/^$/d' | tr '\n' '\0' \
                | xargs -0 -P "${jobs}" -n 1 "${wrapper}" \
                | filter_output \
                | awk -v total="${TOTAL_COUNT}" -v pf="${PROGRESS_FILE}" -v marker="${PROGRESS_MARKER}" '
                    BEGIN { getline done < pf; close(pf); done += 0 }
                    $0 == marker {
                        done++
                        pct = int(done * 100 / total)
                        printf "\r  clang-tidy: %d%% (%d/%d files)", pct, done, total > "/dev/stderr"
                        fflush("/dev/stderr")
                        next
                    }
                    { print }
                    END { print done > pf; close(pf) }
                ' | tee -a "${TIDY_OUTPUT}" > /dev/null
            EXIT_CODE=$((EXIT_CODE | ${PIPESTATUS[3]}))
        fi
        rm -f "${wrapper}"
    else
        printf '%s' "${file_list}" | sed '/^$/d' | tr '\n' '\0' \
            | xargs -0 -P "${jobs}" -n 1 clang-tidy -p "${BUILD_DIR}" "${tidy_args[@]}" ${FIX_ARG} \
            2>&1 | filter_output
        EXIT_CODE=$((EXIT_CODE | ${PIPESTATUS[3]}))
    fi
}

# Lint production code with root .clang-tidy
# Source .cpp files may not all be in the compilation database (e.g., platform
# stubs behind #ifdef guards). Add project include paths as fallback so headers
# resolve. The compilation DB is still consulted first via -p.
CLANG_TIDY_SRC_CPP=( "${CLANG_TIDY_MINIMAL[@]}" "${PROJECT_INCLUDE_ARGS[@]}" "${CLANG_TIDY_DEPS_EXTRAS[@]}" )
if [ -n "${SRC_CPP_FILES}" ]; then
    run_tidy_parallel "${SRC_CPP_FILES}" "${CLANG_TIDY_SRC_CPP[@]}" --config-file="${PROJECT_DIR}/.clang-tidy"
fi
if [ -n "${SRC_H_FILES}" ]; then
    run_tidy_parallel "${SRC_H_FILES}" "${CLANG_TIDY_HEADER_EXTRA[@]}" --config-file="${PROJECT_DIR}/.clang-tidy"
fi

# Lint test code with tests/.clang-tidy
# Test .cpp files may not be in the compilation database (e.g., plugin tests
# not yet hooked into CMake). Add project include paths as fallback so headers
# resolve. The compilation DB is still consulted first via -p.
# Also add _deps includes for third-party headers (nlohmann/json, catch2, etc.)
CLANG_TIDY_TEST_CPP=( "${CLANG_TIDY_MINIMAL[@]}" "${PROJECT_INCLUDE_ARGS[@]}" "${CLANG_TIDY_DEPS_EXTRAS[@]}" )
if [ -n "${TEST_CPP_FILES}" ]; then
    run_tidy_parallel "${TEST_CPP_FILES}" "${CLANG_TIDY_TEST_CPP[@]}" --config-file="${TEST_TIDY_CONFIG}"
fi
if [ -n "${TEST_H_FILES}" ]; then
    run_tidy_parallel "${TEST_H_FILES}" "${CLANG_TIDY_HEADER_EXTRA[@]}" --config-file="${TEST_TIDY_CONFIG}"
fi

# Finish the progress line after all run_tidy_parallel calls
if [ "${CHECKLIST}" = true ]; then
    printf '\r  clang-tidy: 100%% (%d/%d files)\n' "${TOTAL_COUNT}" "${TOTAL_COUNT}" >&2
fi

# In checklist mode, generate the markdown checklist from collected output
if [ "${CHECKLIST}" = true ]; then
    if generate_checklist "${TIDY_OUTPUT}" "${PROJECT_DIR}" "${CHECKLIST_FILE}"; then
        if [ "${QUIET}" != true ]; then
            echo ""
            echo "Checklist written to ${CHECKLIST_FILE}"
        fi
        exit 0
    else
        echo "Error: checklist generation failed" >&2
        exit 1
    fi
fi

exit ${EXIT_CODE}
