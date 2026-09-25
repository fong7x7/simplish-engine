#pragma once

/// @file logic-event-kind.h
/// @brief What happened, in an event game logic is told of.
/// @par Threading
/// A value type.

#include <cstdint>

namespace eng::game {

/// Everything `GameLogicWorld::events` reports.
enum class LogicEventKind : uint8_t {
  /// An actor joined the run: spawned by the logic.
  ACTOR_SPAWNED,
  /// An actor was hurt, and lived.
  ACTOR_HURT,
  /// An actor was killed — by anything: a shot, a blast, the logic.
  ACTOR_DIED,
  /// An actor was taken out of the run by the logic, without dying.
  ACTOR_REMOVED,
  /// A player was hurt, and is still up.
  PLAYER_HURT,
  /// A player went down.
  PLAYER_DOWNED,
};

}  // namespace eng::game
