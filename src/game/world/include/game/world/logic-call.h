#pragma once

/// @file logic-call.h
/// @brief Which of the game logic's entry points a world calls.
/// @par Threading
/// A value type.

#include <cstdint>

namespace eng::game {

/// What `GameWorld` is calling the game logic for.
enum class LogicCall : uint8_t {
  /// The director's phase: `start` on tick 0, then `tick`.
  TICK,
  /// The run is over: `end`, once.
  END,
};

}  // namespace eng::game
