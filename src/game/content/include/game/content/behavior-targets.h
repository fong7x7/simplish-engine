#pragma once

/// @file behavior-targets.h
/// @brief Whom an actor takes as its target.
/// @par Threading
/// A value type.

#include <cstdint>

namespace eng::game {

/// Whom an actor looks for a target among.
enum class BehaviorTargets : uint8_t {
  /// The players, whatever side it is on: a hostile actor hunts them, a
  /// friendly one keeps to them, and its behavior says which.
  PLAYERS,
  /// Its opponents: for a hostile actor the players and friendly actors,
  /// for a friendly one hostile actors. A guard that fights raiders.
  OPPONENTS,
};

}  // namespace eng::game
