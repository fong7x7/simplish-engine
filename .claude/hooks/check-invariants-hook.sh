#!/usr/bin/env bash
# PostToolUse hook: run the Simplish coding invariants on a file Claude just
# edited, and feed any violations back so they are fixed in the same turn
# rather than at lint time.
#
# Reads the hook payload on stdin, acts only on C++ sources under src/, and
# exits 2 with the violations on stderr (the PostToolUse contract for
# "tell the model about this"). Anything else exits 0 and stays silent.
set -uo pipefail

PROJECT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"

file="$(jq -r '.tool_response.filePath // .tool_input.file_path // empty')"
[ -n "${file}" ] || exit 0
[ -f "${file}" ] || exit 0

case "${file}" in
  *.h|*.hpp|*.cpp|*.mm) ;;
  *) exit 0 ;;
esac
case "${file}" in
  "${PROJECT_DIR}"/src/*) ;;
  *) exit 0 ;;
esac

violations="$(python3 "${PROJECT_DIR}/scripts/check_all.py" -q "${file}" 2>&1)" && exit 0

[ -n "${violations}" ] || exit 0
{
  echo "Coding invariants failed for ${file}:"
  echo "${violations}"
  echo
  echo "Fix these before moving on. See CLAUDE.md § Invariants. A suppression"
  echo "needs // NOLINTNEXTLINE(check-name) with a reason on the line above."
} >&2
exit 2
