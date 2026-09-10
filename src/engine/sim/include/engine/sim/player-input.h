#pragma once

/// @file player-input.h
/// @brief One player's input for one tick.
/// @par Threading
/// A value type.

#include <cstdint>
#include <type_traits>

namespace eng::sim {

/// What one player did during one tick — the unit lockstep peers exchange
/// (ADR-005) and a replay records (Engine REQUIREMENTS §4.4).
///
/// Every field is an integer, quantised by whatever captures it. A float
/// here would reach two peers as two different roundings of the same stick
/// position, and the session would desync on the first tick it mattered.
struct PlayerInput {
  /// Movement stick X: -32767 is full left, 32767 full right.
  int16_t move_x = 0;
  /// Movement stick Y, on the same scale as `move_x`.
  int16_t move_y = 0;
  /// Aim direction X, on the same scale; the game decides what (0, 0) means.
  int16_t aim_x = 0;
  /// Aim direction Y, on the same scale as `aim_x`.
  int16_t aim_y = 0;
  /// Held buttons, one bit per game-defined action.
  uint32_t buttons = 0;

  /// Inputs are equal when every field is.
  bool operator==(const PlayerInput&) const = default;
};

static_assert(std::has_unique_object_representations_v<PlayerInput>,
              "PlayerInput is hashed and serialised as its bytes; padding "
              "would put indeterminate bytes into both");

}  // namespace eng::sim
