#pragma once

/// @file logic-player-status.h
/// @brief Whether a player is up, down or out, as game logic reads it.
/// @par Threading
/// A value type.

#include <cstdint>

namespace eng::game {

/// Where a player stands in the run (Game §3.3).
enum class LogicPlayerStatus : uint8_t {
  /// Up and playing.
  UP,
  /// Out of health, waiting for a teammate to revive them.
  DOWN,
  /// Out of the run: down too long, or with nobody left to revive them.
  OUT,
};

}  // namespace eng::game
