#pragma once

/// @file logic-steps.h
/// @brief Whose steps game logic hears.
/// @par Threading
/// A value type.

#include <cstdint>

namespace eng::game {

/// Whose footsteps the world reports to the logic as `PLAYER_STEPPED` and
/// `ACTOR_STEPPED` events: a step each time a walker covers its feet's
/// stride. Off unless asked for — two thousand actors walking would be
/// hundreds of events a tick.
enum class LogicSteps : uint8_t {
  /// Nobody's.
  NONE,
  /// The players' only.
  PLAYERS,
  /// The players' and every actor's.
  EVERYONE,
};

}  // namespace eng::game
