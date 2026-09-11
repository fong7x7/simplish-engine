#pragma once

/// @file behavior-condition.h
/// @brief What must be true for an actor to leave a state.
/// @par Threading
/// A value type.

#include <cstdint>

namespace eng::game {

/// The closed set of conditions a behavior's exits test (ADR-009). Each
/// reads what the actor perceived this tick and what its movement did last
/// tick — never anything it could not know.
enum class BehaviorCondition : uint8_t {
  /// Always true: leave at the first chance.
  ALWAYS,
  /// The actor can see its target: in range, in its view cone, nothing
  /// solid in between.
  SEES_TARGET,
  /// The actor heard its target fire, within hearing range; walls do not
  /// stop sound.
  HEARS_TARGET,
  /// The actor has not perceived its target for `ticks` ticks — or has
  /// never perceived one.
  LOST_TARGET_FOR,
  /// Where the target was seen is within `tiles` of the actor.
  TARGET_WITHIN,
  /// The actor has a target, seen further than `tiles` away.
  TARGET_BEYOND,
  /// The actor has been in this state for `ticks` ticks.
  IN_STATE_FOR,
  /// The actor reached where its action was taking it.
  ARRIVED,
  /// The actor's action wants to go somewhere no path reaches.
  NO_PATH,
  /// The actor tried to move and something solid stopped it.
  BLOCKED,
  /// The actor is more than `tiles` from where it spawned.
  FAR_FROM_HOME,
  /// A draw from the simulation's AI stream came in under `permille`: a
  /// chance per tick, the same on every peer.
  CHANCE,
  /// The actor was hurt within the last `ticks` ticks (at least one).
  DAMAGED,
  /// The actor's health is below `permille` thousandths of its full health.
  HEALTH_BELOW,
  /// Another actor on its side stands within `tiles` of it.
  ALLIES_WITHIN,
};

}  // namespace eng::game
