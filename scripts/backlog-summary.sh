#!/usr/bin/env bash
set -euo pipefail

# backlog-summary.sh — Print a summary of .autocode/backlog.json
# Shows enabled/disabled/blocked counts, actionable items, and dependency chains.

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
cd "$SCRIPT_DIR/.."

BACKLOG_FILE=".autocode/backlog.json"
RESULTS_DIR=".autocode/results"

if [[ ! -f "$BACKLOG_FILE" ]]; then
  echo "No backlog file found at $BACKLOG_FILE" >&2
  exit 1
fi

total=$(jq 'length' "$BACKLOG_FILE")
enabled_count=$(jq '[.[] | select(.enabled != false)] | length' "$BACKLOG_FILE")
disabled_count=$((total - enabled_count))
done_count=$(jq '[.[] | select(.description | startswith("[DONE]"))] | length' "$BACKLOG_FILE")

# Build set of done task IDs (disabled or completed result)
done_ids=$(jq -r '.[] | select(.enabled == false) | .id' "$BACKLOG_FILE")
if [[ -d "$RESULTS_DIR" ]]; then
  for rf in "$RESULTS_DIR"/*.json; do
    [[ -f "$rf" ]] || continue
    if [[ "$(jq -r '.status' "$rf" 2>/dev/null)" == "completed" ]]; then
      done_ids+=$'\n'"$(basename "$rf" .json)"
    fi
  done
fi

# Classify enabled items as actionable or blocked
actionable_ids=()
blocked_ids=()
enabled_ids=$(jq -r '.[] | select(.enabled != false) | .id' "$BACKLOG_FILE")
while IFS= read -r tid; do
  [[ -z "$tid" ]] && continue
  # Skip already completed
  if [[ -d "$RESULTS_DIR" ]] && [[ -f "$RESULTS_DIR/${tid}.json" ]]; then
    status=$(jq -r '.status' "$RESULTS_DIR/${tid}.json" 2>/dev/null)
    [[ "$status" == "completed" ]] && continue
  fi
  # Check dependencies
  deps=$(jq -r --arg tid "$tid" '.[] | select(.id == $tid) | .depends_on // [] | .[]' "$BACKLOG_FILE" 2>/dev/null)
  met=true
  if [[ -n "$deps" ]]; then
    while IFS= read -r dep; do
      [[ -z "$dep" ]] && continue
      if ! echo "$done_ids" | grep -qxF "$dep"; then
        met=false
        break
      fi
    done <<< "$deps"
  fi
  if [[ "$met" == "true" ]]; then
    actionable_ids+=("$tid")
  else
    blocked_ids+=("$tid")
  fi
done <<< "$enabled_ids"

# Skill counts (bash 3.2 compatible — no associative arrays)
skill_summary=$(jq -r '.[] | select(.enabled != false) | .skill // "unknown"' "$BACKLOG_FILE" \
  | sort | uniq -c | sort -rn \
  | awk '{if(NR>1) printf ", "; printf "%s: %d", $2, $1}')

# Print summary
echo "## Backlog Summary"
echo ""
echo "- Total: $total"
echo "- Enabled: $enabled_count (Actionable: ${#actionable_ids[@]}, Blocked: ${#blocked_ids[@]})"
echo "- Disabled: $disabled_count (Done: $done_count)"
echo "- By skill: $skill_summary"
echo ""

# Actionable items
echo "### Actionable Now (${#actionable_ids[@]})"
echo ""
if [[ ${#actionable_ids[@]} -gt 0 ]]; then
  echo "| ID | Skill | Description |"
  echo "|----|-------|-------------|"
  for tid in "${actionable_ids[@]}"; do
    skill=$(jq -r --arg tid "$tid" '.[] | select(.id == $tid) | .skill // "-"' "$BACKLOG_FILE")
    desc=$(jq -r --arg tid "$tid" '.[] | select(.id == $tid) | .description // "-"' "$BACKLOG_FILE")
    echo "| $tid | $skill | ${desc:0:60} |"
  done
fi
echo ""

# Blocked items
echo "### Blocked (${#blocked_ids[@]})"
echo ""
if [[ ${#blocked_ids[@]} -gt 0 ]]; then
  echo "| ID | Waiting On |"
  echo "|----|------------|"
  for tid in "${blocked_ids[@]}"; do
    deps=$(jq -r --arg tid "$tid" '.[] | select(.id == $tid) | .depends_on // [] | .[]' "$BACKLOG_FILE" 2>/dev/null)
    unmet=""
    while IFS= read -r dep; do
      [[ -z "$dep" ]] && continue
      if ! echo "$done_ids" | grep -qxF "$dep"; then
        [[ -n "$unmet" ]] && unmet+=", "
        unmet+="$dep"
      fi
    done <<< "$deps"
    echo "| $tid | $unmet |"
  done
fi
