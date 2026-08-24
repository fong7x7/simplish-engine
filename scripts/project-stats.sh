#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
cd "$SCRIPT_DIR/.."

EXCLUDE=(-path ./.git -o -path ./build -o -path ./.claude/worktrees)

count_files() {
  find . \( "${EXCLUDE[@]}" \) -prune -o -type f -name "$1" -print | wc -l | tr -d ' '
}

count_lines() {
  find . \( "${EXCLUDE[@]}" \) -prune -o -type f -name "$1" -print0 \
    | xargs -0 wc -l 2>/dev/null \
    | awk '!/total$/{s+=$1} END {print s+0}'
}

sum_grep_hits() {
  find . \( "${EXCLUDE[@]}" \) -prune -o -type f -name "test_*.cpp" -print0 \
    | xargs -0 grep -c "$1" 2>/dev/null \
    | awk -F: '{s+=$NF} END {print s+0}'
}

header_files=$(count_files "*.h")
source_files=$(count_files "*.cpp")
total_files=$((header_files + source_files))

header_lines=$(count_lines "*.h")
source_lines=$(count_lines "*.cpp")
total_lines=$((header_lines + source_lines))

test_files=$(count_files "test_*.cpp")
test_cases=$(sum_grep_hits "TEST_CASE")
sections=$(sum_grep_hits "SECTION")

cmake_files=$(count_files "CMakeLists.txt")
md_files=$(count_files "*.md")
json_files=$(count_files "*.json")

printf "\n"
printf "  Simplish Project Stats\n"
printf "  ══════════════════════\n\n"
printf "  %-28s %s\n" "Header files (.h)" "$header_files"
printf "  %-28s %s\n" "Source files (.cpp)" "$source_files"
printf "  %-28s %s\n" "Total C++ files" "$total_files"
printf "\n"
printf "  %-28s %s\n" "Header lines" "$header_lines"
printf "  %-28s %s\n" "Source lines" "$source_lines"
printf "  %-28s %s\n" "Total C++ lines" "$total_lines"
printf "\n"
printf "  %-28s %s\n" "Test files (test_*.cpp)" "$test_files"
printf "  %-28s %s\n" "TEST_CASE count" "$test_cases"
printf "  %-28s %s\n" "SECTION count" "$sections"
printf "\n"
printf "  %-28s %s\n" "CMakeLists.txt files" "$cmake_files"
printf "  %-28s %s\n" "Markdown files (.md)" "$md_files"
printf "  %-28s %s\n" "JSON data files (.json)" "$json_files"
printf "\n"
