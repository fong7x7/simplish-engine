#pragma once

/// @file combat-workspace.h
/// @brief Scratch the combat phases share, rebuilt every tick.
/// @par Threading
/// Main-thread-only; used only inside a tick.

#include <cstdint>
#include <engine/math/vec2.h>
#include <engine/physics/box-broadphase.h>
#include <engine/spatial/nav-grid-spec.h>
#include <engine/spatial/neighbor-grid.h>
#include <game/combat/combat-body.h>
#include <vector>

namespace eng::game {

/// Everyone who can be hurt this tick, and a neighbour grid over them, so
/// a shot asks who is near its path rather than asking everyone. None of it
/// is state: the world lists the bodies afresh each tick.
struct CombatWorkspace {
  /// Scratch for up to @p bodies bodies over the floor @p floor covers,
  /// colliding with the boxes of @p broadphase.
  CombatWorkspace(uint32_t bodies, const spatial::NavGridSpec& floor,
                  const physics::BoxBroadphase& broadphase);

  /// Everyone who can be hurt, as the world listed them.
  std::vector<CombatBody> bodies;
  /// Where each body stands, for the grid.
  std::vector<Vec2> points;
  /// The bodies, bucketed.
  spatial::NeighborGrid grid;
  /// The widest body's radius.
  float largest_radius = 0.0F;
  /// The boxes near a projectile's step, as the broadphase gathers them.
  std::vector<uint32_t> boxes;
};

/// Bucket @p workspace's bodies, once the world has listed them.
void indexCombatBodies(CombatWorkspace& workspace);

}  // namespace eng::game
