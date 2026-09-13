#pragma once

/// @file combat-scene.h
/// @brief One tick's view of the world, as the combat phases see it.
/// @par Threading
/// A view over the world's state for one tick.

#include <cstdint>
#include <engine/physics/box-broadphase.h>
#include <engine/physics/collision-box.h>
#include <game/combat/combat-cue.h>
#include <game/combat/combat-effects.h>
#include <game/combat/combat-workspace.h>
#include <span>
#include <vector>

namespace eng::game {

/// What projectiles, hazards and blasts need of the world for one tick: the
/// clock, the level's boxes, everyone who can be hurt, the effects buffer
/// their hits go into, and the cues that report them. Built by the world and
/// discarded with the tick.
struct CombatScene {
  /// The tick being simulated.
  uint64_t tick = 0;
  /// The level's solid geometry: what stops a shot.
  std::span<const physics::CollisionBox> obstacles;
  /// `obstacles`, bucketed.
  const physics::BoxBroadphase& broadphase;
  /// Everyone who can be hurt, and the scratch to find them with.
  CombatWorkspace& workspace;
  /// Where hits go.
  CombatEffects& effects;
  /// Where what presentation wants to know about goes: a projectile's hit,
  /// a blast. Only appended to, and only into room already reserved.
  std::vector<CombatCue>& cues;
};

}  // namespace eng::game
