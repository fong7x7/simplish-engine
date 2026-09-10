#pragma once

/// @file scripted-input.h
/// @brief A deterministic stand-in for two players at a controller.
/// @par Threading
/// Pure function.

#include <cstdint>
#include <engine/sim/tick-input.h>

namespace eng::sim::testing {

/// Players in `scriptedInput`'s session.
inline constexpr uint8_t SCRIPTED_PLAYERS = 2;

/// Input for `tick` from two scripted players: player 0 fires every seventh
/// tick with a sweeping aim, player 1 strafes and fires every eleventh.
TickInput scriptedInput(uint64_t tick);

}  // namespace eng::sim::testing
