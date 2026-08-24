#!/usr/bin/env bash
set -euo pipefail

# implementation-gaps.sh — Combined report of implementation gaps.
# Runs stub-finder, untested-files, and checks requirement-to-code mapping.

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
cd "$SCRIPT_DIR/.."

echo "# Implementation Gaps Report"
echo ""
echo "Generated: $(date -u '+%Y-%m-%d %H:%M UTC')"
echo ""

# --- Section 1: Requirements without source directories ---
echo "## 1. Requirements Without Source Code"
echo ""
echo "Requirements docs in docs/engine/ with no corresponding src/engine/ package:"
echo ""

# Known doc-to-source name mappings (doc name → source dir name)
# and names to skip (architecture refs, sub-topic files, hub files)
is_known_or_skip() {
  case "$1" in
    # Name mappings: doc name differs from source dir
    rendering|render) return 0 ;;  # engine/render/
    voxels|voxel) return 0 ;;      # engine/voxel/
    audit) return 0 ;;             # lives in engine/core/
    # Architecture/reference docs, not subsystem requirements
    architecture) return 0 ;;
    # Hub and review files
    REQUIREMENTS|REQUIREMENTS-REVIEW) return 0 ;;
    # Sub-files of existing requirements (e.g., data-formats-REQUIREMENTS)
    *-REQUIREMENTS) return 0 ;;
    # Sub-topic files that belong to a parent doc dir
    forces-*|particles-*|projectiles-*|hitboxes-*|lod-*|propagation-*) return 0 ;;
  esac
  return 1
}

# Check if a doc name maps to an existing source directory
has_source() {
  local name="$1"
  # Direct match
  [[ -d "src/engine/$name" ]] && return 0
  # Known mappings
  case "$name" in
    rendering) [[ -d "src/engine/render" ]] && return 0 ;;
    gui) [[ -d "src/engine/gui" ]] && return 0 ;;
  esac
  # Try singular/plural variants
  [[ -d "src/engine/${name%s}" ]] && return 0
  [[ -d "src/engine/${name}s" ]] && return 0
  return 1
}

gaps_found=0
seen_gaps=""

# Scan both dirs and files, deduplicate
for req_path in docs/engine/*/ docs/engine/*.md; do
  if [[ -d "$req_path" ]]; then
    subsystem=$(basename "$req_path")
  elif [[ -f "$req_path" ]]; then
    subsystem=$(basename "$req_path" .md)
  else
    continue
  fi

  # Skip known non-subsystem entries
  is_known_or_skip "$subsystem" && continue

  # Skip already reported
  echo "$seen_gaps" | grep -qxF "$subsystem" && continue

  if ! has_source "$subsystem"; then
    echo "- **$subsystem**: has \`docs/engine/$subsystem\` but no matching \`src/engine/\` package"
    gaps_found=$((gaps_found + 1))
    seen_gaps+=$'\n'"$subsystem"
  fi
done

[[ $gaps_found -eq 0 ]] && echo "None found."
echo ""

# --- Section 2: Run stub finder ---
echo "## 2. Stubs & TODOs"
echo ""
bash "$SCRIPT_DIR/stub-finder.sh" | tail -n +3
echo ""

# --- Section 3: Run untested files ---
echo "## 3. Test Coverage Gaps"
echo ""
bash "$SCRIPT_DIR/untested-files.sh" | tail -n +3
echo ""

# --- Section 4: Tech approach gaps ---
echo "## 4. Requirements Without Technical Approaches"
echo ""

ta_gaps=0
for req_file in docs/editor/*/REQUIREMENTS.md; do
  [[ -f "$req_file" ]] || continue
  panel=$(basename "$(dirname "$req_file")")
  ta_dir="docs/technical-approaches/editor/$panel"
  if [[ ! -d "$ta_dir" ]] && [[ ! -f "$ta_dir.md" ]]; then
    echo "- **editor/$panel**: has requirements but no Phase 1 technical approach"
    ta_gaps=$((ta_gaps + 1))
  fi
done

[[ $ta_gaps -eq 0 ]] && echo "None found."
echo ""

echo "---"
echo "Run \`./scripts/backlog-summary.sh\` for current backlog state."
