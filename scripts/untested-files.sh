#!/usr/bin/env bash
set -euo pipefail

# untested-files.sh — Find source files with no corresponding test file.
# For each .cpp under src/, checks whether a matching test_*.cpp exists in
# some package's test/ directory (docs/development/code-layout.md).

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
cd "$SCRIPT_DIR/.."

DIRS=(src/engine src/platform src/editor src/game src/bin)
EXCLUDE_DIRS="build|_deps|CMakeFiles|\.claude|/test/"

echo "## Untested Source Files"
echo ""

total_untested=0
total_source=0

for dir in "${DIRS[@]}"; do
  [[ -d "$dir" ]] || continue

  untested_files=()

  while IFS= read -r src_file; do
    [[ -z "$src_file" ]] && continue
    total_source=$((total_source + 1))

    # Extract the base name without path/extension
    base=$(basename "$src_file" .cpp)
    # Normalise hyphens in source names to underscores; project test files
    # use underscore-only names (e.g. input-action.cpp -> test_input_action.cpp).
    base_underscore=${base//-/_}

    # Look for test_<base>.cpp or test_<base_underscore>.cpp in any test/ dir
    test_match=$(find src/ -path '*/test/*' \( -name "test_${base}.cpp" -o \
                                -name "test_${base_underscore}.cpp" \) \
                 2>/dev/null | head -1)
    if [[ -z "$test_match" ]]; then
      untested_files+=("$src_file")
    fi
  done < <(find "$dir" -name '*.cpp' -not -path '*/test_*' \
    | grep -Ev "$EXCLUDE_DIRS" \
    | sort)

  count=${#untested_files[@]}
  if [[ $count -gt 0 ]]; then
    echo "### $dir/ ($count untested)"
    echo ""
    for f in "${untested_files[@]}"; do
      echo "- $f"
    done
    echo ""
    total_untested=$((total_untested + count))
  fi
done

echo "---"
echo ""
echo "**Total source files: $total_source**"
echo "**Untested: $total_untested**"
if [[ $total_source -gt 0 ]]; then
  pct=$(( (total_source - total_untested) * 100 / total_source ))
  echo "**File-level test coverage: ${pct}%**"
fi
