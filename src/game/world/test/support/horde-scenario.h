#pragma once

/// @file horde-scenario.h
/// @brief The fixed full-load scene the horde budget is measured against:
/// four players in a pillared arena and a horde of swarmers round them.
/// @par Threading
/// Test code; one thread.

#include <cstdint>
#include <engine/sim/tick-input.h>
#include <game/content/game-content.h>
#include <game/world/game-setup.h>

namespace eng::game::test {

/// The swarmers Engine §7 budgets enemy AI for.
inline constexpr uint32_t HORDE_ACTOR_COUNT = 2000;

/// The arena: @p actors swarmers — the archetype `hordeContent` defines —
/// scattered through the outer ring of a
/// walled 64 × 64-tile room studded with pillars, and four players in its
/// middle. The same every time it is asked for.
[[nodiscard]] GameSetup hordeSetup(uint32_t actors);

/// The content the horde runs: a swarmer archetype running the built-in
/// chase behavior, seeing far enough that every swarmer in the room is
/// after a player — the worst case, and what a director spawning at a
/// player's position produces.
[[nodiscard]] GameContent hordeContent();

/// The players' input on @p tick: each circling at their own pace, firing
/// now and then.
[[nodiscard]] sim::TickInput hordeInput(uint64_t tick);

}  // namespace eng::game::test
