#!/usr/bin/env bash
set -euo pipefail

# stub-finder.sh — Find stub patterns and TODO/FIXME comments in source files.
# Scans src/ for incomplete implementations, skipping package test/ folders.

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
cd "$SCRIPT_DIR/.."

DIRS=(src)
EXCLUDE_DIRS="build|_deps|CMakeFiles|\.claude|/test/"

# Helper: grep that never fails (returns empty on no match)
safe_grep() {
  grep "$@" 2>/dev/null || true
}

echo "## Stub & TODO Report"
echo ""

# --- TODO / FIXME ---
echo "### TODO/FIXME Comments"
echo ""
total_todos=0
for dir in "${DIRS[@]}"; do
  [[ -d "$dir" ]] || continue
  matches=$(safe_grep -rn --include='*.cpp' --include='*.h' -E '\b(TODO|FIXME)\b' "$dir" \
    | safe_grep -Ev "$EXCLUDE_DIRS")
  count=$(echo "$matches" | safe_grep -c . )
  if [[ "$count" -gt 0 ]]; then
    echo "**$dir/** ($count)"
    echo "$matches" | sed 's/^/  /' | head -20
    remaining=$((count - 20))
    [[ $remaining -gt 0 ]] && echo "  ... and $remaining more"
    echo ""
    total_todos=$((total_todos + count))
  fi
done
echo "**Total: $total_todos**"
echo ""

# --- Stub patterns ---
echo "### Stub Patterns (return nullptr / return false / return 0)"
echo ""
total_stubs=0
for dir in "${DIRS[@]}"; do
  [[ -d "$dir" ]] || continue
  hits=$(safe_grep -rn --include='*.cpp' -E 'return (nullptr|false|0);' "$dir" \
    | safe_grep -Ev "$EXCLUDE_DIRS" \
    | safe_grep -Ev 'test_|_test\.' \
    | safe_grep -Ev '(find|tryGet|tryFind|lookup|contains)\(')
  count=$(echo "$hits" | safe_grep -c .)
  if [[ "$count" -gt 0 ]]; then
    echo "**$dir/** ($count)"
    echo "$hits" | sed 's/^/  /' | head -20
    remaining=$((count - 20))
    [[ $remaining -gt 0 ]] && echo "  ... and $remaining more"
    echo ""
    total_stubs=$((total_stubs + count))
  fi
done
echo "**Total: $total_stubs**"
echo ""

# --- Explicit stub markers ---
echo "### Explicit Stub Markers"
echo ""
markers=$(safe_grep -rn --include='*.cpp' --include='*.h' \
  -E '(TODO-stub|PLACEHOLDER|not.implemented|NOT_IMPLEMENTED)' \
  "${DIRS[@]}" \
  | safe_grep -Ev "$EXCLUDE_DIRS")
count=$(echo "$markers" | safe_grep -c .)
if [[ "$count" -gt 0 ]]; then
  echo "$markers" | sed 's/^/  /'
else
  echo "None found."
fi
echo ""
echo "**Total explicit stubs: $count**"
