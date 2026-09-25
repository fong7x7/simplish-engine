#pragma once

/// @file logic-command-kind.h
/// @brief What one write of a project's game logic does.
/// @par Threading
/// A value type.

#include <cstdint>

namespace eng::game {

/// The writes `GameLogicWorld` queues.
enum class LogicCommandKind : uint8_t {
  /// Take health.
  DAMAGE,
  /// Give health back.
  HEAL,
  /// Put a player or an actor somewhere else.
  MOVE,
  /// Take an actor out of the run, without its dying.
  REMOVE,
  /// Put an actor in another state of its behavior.
  SET_STATE,
  /// Put an actor on another side.
  SET_FACTION,
};

}  // namespace eng::game
