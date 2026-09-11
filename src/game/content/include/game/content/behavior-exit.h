#pragma once

/// @file behavior-exit.h
/// @brief One way out of a behavior state: a condition and where it leads.
/// @par Threading
/// A value type.

#include <cstdint>
#include <game/content/behavior-condition.h>

namespace eng::game {

/// A transition: when `when` holds, the actor enters state `to`.
///
/// A condition reads at most one of `tiles`, `ticks` and `permille`,
/// whichever its kind names; the others are ignored. Exits are tested in
/// the order they are written, and the first that holds is taken — so
/// order is how a behavior says which situation matters most.
struct BehaviorExit {
  /// The condition that takes this exit.
  BehaviorCondition when = BehaviorCondition::ALWAYS;
  /// A distance the condition compares against, in tiles.
  float tiles = 0.0F;
  /// A duration the condition compares against, in ticks.
  uint32_t ticks = 0;
  /// A chance, in thousandths.
  uint16_t permille = 0;
  /// The state it leads to, as an index into the behavior's states.
  uint8_t to = 0;
};

}  // namespace eng::game
