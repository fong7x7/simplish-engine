#!/usr/bin/env bash
# Run all CLAUDE.md coding invariant checks and produce a unified summary.
#
# This is a thin wrapper around the Python unified checker (check_all.py).
# All logic now lives in check_all.py for performance (single process,
# single file-discovery pass, each file read once).
#
# Usage: scripts/check-invariants.sh [OPTIONS] [PATH...]
#
# Options:
#   --check <name>     Run only the named check (repeat for several)
#   --list             List all available checks
#   --src-only         Check only production code (not with --tests-only)
#   --tests-only       Check only test code (not with --src-only)
#   --checklist [FILE] Output markdown checklist (default: invariant-checklist.md)
#   -q, --quiet        Only print violations (no progress or summary)
#   --progress         Show file progress even when --quiet is set
set -eo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

exec python3 "${SCRIPT_DIR}/check_all.py" "$@"
