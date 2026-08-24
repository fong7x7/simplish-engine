#!/usr/bin/env bash
set -euo pipefail

usage() {
  echo "Usage: $0 <file.json> [more.json ...]"
  echo
  echo "Validates JSON files using jq."
  echo "Returns non-zero if any file is invalid."
}

if ! command -v jq >/dev/null 2>&1; then
  echo "Error: jq is required but not installed." >&2
  exit 2
fi

if [[ $# -lt 1 ]]; then
  usage
  exit 1
fi

exit_code=0

for file in "$@"; do
  if [[ ! -f "$file" ]]; then
    echo "❌ $file: file not found"
    exit_code=1
    continue
  fi

  if jq empty "$file" >/dev/null 2>&1; then
    echo "✅ $file: valid JSON"
  else
    echo "❌ $file: invalid JSON"
    exit_code=1
  fi
done

exit "$exit_code"