#!/bin/bash
# ─────────────────────────────────────────────────────────────────────────────
# perf-gate.sh — Measure the simulation's budgets on an optimised build
#
# Usage:
#   ./scripts/perf-gate.sh                     # Build relwithdebinfo, run the gate
#   ./scripts/perf-gate.sh --no-build          # Run what is already built
#
# Development REQUIREMENTS §6: a budget is measured, not assumed. Today the
# gate is the horde — 2,000 actors in a pillared arena closing on four
# players — held to Engine §7's 2.5 ms a tick for enemy AI and steering,
# median over 600 ticks. It prints each phase's median, 99th percentile and
# worst tick, and fails when the actors' median is over budget. Then the
# water: the largest ripple field stepped and packed a frame, held to its
# 1 ms share of render submission (docs/engine/water.md §6).
#
# The timing cases are hidden Catch2 tests tagged [perf]: they mean nothing
# in a debug build, so ctest never runs them. Numbers are only comparable
# on the same machine; the pinned CI runner class the requirements name is
# not set up yet.
# ─────────────────────────────────────────────────────────────────────────────
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_DIR="$(cd "${SCRIPT_DIR}/.." && pwd)"
cd "${PROJECT_DIR}"

PRESET="relwithdebinfo"
TARGET="simplish-game-world-tests"
WATER_TARGET="simplish-engine-render-water-tests"
DO_BUILD=true

for arg in "$@"; do
    case "${arg}" in
        --no-build) DO_BUILD=false ;;
        -h|--help)
            sed -n '2,20p' "$0"
            exit 0
            ;;
        *)
            echo "perf-gate.sh: unknown argument '${arg}'" >&2
            exit 2
            ;;
    esac
done

if [[ "${DO_BUILD}" == true ]]; then
    cmake --preset "${PRESET}" > /dev/null
    cmake --build --preset "${PRESET}" --target "${TARGET}" "${WATER_TARGET}"
fi

BINARY="build/${PRESET}/src/game/world/${TARGET}"
WATER_BINARY="build/${PRESET}/src/engine/render-water/${WATER_TARGET}"
for binary in "${BINARY}" "${WATER_BINARY}"; do
    if [[ ! -x "${binary}" ]]; then
        echo "perf-gate.sh: ${binary} is not built; run without --no-build" >&2
        exit 1
    fi
done

"${BINARY}" "[perf]"
"${WATER_BINARY}" "[perf]"
