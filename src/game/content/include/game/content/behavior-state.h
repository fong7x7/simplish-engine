#pragma once

/// @file behavior-state.h
/// @brief One state of a behavior: what the actor does, and its ways out.
/// @par Threading
/// A value type.

#include <cstdint>
#include <game/content/behavior-action.h>
#include <game/content/behavior-exit.h>
#include <game/content/behavior-facing.h>
#include <game/content/behavior-route-mode.h>
#include <string>
#include <vector>

namespace eng::game {

/// Full speed, in the thousandths `speed_permille` is counted in.
inline constexpr uint16_t BEHAVIOR_FULL_SPEED_PERMILLE = 1000;

/// A state of a behavior's state machine.
struct BehaviorState {
  /// What the state is called, unique within its behavior: `pursue`.
  std::string id{};
  /// What the actor does while in it.
  BehaviorAction action = BehaviorAction::IDLE;
  /// What the actor turns to face while in it.
  BehaviorFacing facing = BehaviorFacing::MOVEMENT;
  /// The actor's speed while in it, as thousandths of its behavior's speed.
  uint16_t speed_permille = BEHAVIOR_FULL_SPEED_PERMILLE;
  /// The action's near distance, in tiles: how far short of the target a
  /// pursuit or a follow stops, and the inner edge of keeping distance.
  float near_tiles = 0.0F;
  /// The action's far distance, in tiles: how far a wander strays from
  /// home, how far a flight goes, the outer edge of keeping distance.
  float far_tiles = 0.0F;
  /// What a patrol does at the end of its route; ignored by every other
  /// action.
  BehaviorRouteMode route = BehaviorRouteMode::LOOP;
  /// The animation clip a rigged actor plays in this state, or empty for
  /// its walk or idle clip. Presentation: the simulation never reads it.
  std::string clip{};
  /// The ways out, tested in order.
  std::vector<BehaviorExit> exits{};
};

}  // namespace eng::game
