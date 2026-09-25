#pragma once

/// @file logic-target.h
/// @brief A player or an actor, as game logic names it across ticks.
/// @par Threading
/// A value type.

#include <cstdint>
#include <game/logic/logic-target-kind.h>

namespace eng::game {

/// One player or actor, by pool and generational handle. Stable for the
/// entity's life: keep one from a tick to the next and it still names the
/// same entity, or — once that entity is gone — nothing at all, never
/// whoever took its place. Every read `GameLogicWorld` gives carries one,
/// and every write takes one.
///
/// Plain numbers rather than the pools' own handle type, so a project's
/// logic compiles against this package's headers alone.
struct LogicTarget {
  /// Which pool it is in.
  LogicTargetKind kind = LogicTargetKind::PLAYER;
  /// Its slot in that pool.
  uint32_t index = 0;
  /// Which use of that slot it is: bumped every time the slot is reused.
  uint32_t generation = 0;

  /// Two targets are equal when they name the same entity.
  bool operator==(const LogicTarget&) const = default;
};

}  // namespace eng::game
