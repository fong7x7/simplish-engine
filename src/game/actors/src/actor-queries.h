#pragma once

/// @file actor-queries.h
/// @brief Small questions every actor pass asks.
/// @par Threading
/// Pure functions over the tick's state.

#include "actor-ref.h"

#include <cstdint>
#include <engine/math/vec2.h>
#include <engine/math/vec3.h>
#include <engine/spatial/grid-cell.h>
#include <engine/spatial/nav-grid.h>
#include <game/actors/actor-brain.h>
#include <game/actors/actor-tick-context.h>
#include <game/content/behavior-state.h>
#include <optional>

namespace eng::game {

/// @p v on the floor: its X and Y.
[[nodiscard]] Vec2 flat(Vec3 v);

/// The brain actor @p a runs.
[[nodiscard]] const ActorBrain& brainOf(const ActorRef& a,
                                        const ActorTickContext& context);

/// The state of its behavior actor @p a is in.
[[nodiscard]] const BehaviorState& stateOf(const ActorRef& a,
                                           const ActorTickContext& context);

/// The clearance actor @p a needs to stand in a cell of @p grid.
[[nodiscard]] uint8_t clearanceOf(const ActorRef& a,
                                  const spatial::NavGrid& grid);

/// The nearest cell of @p grid to @p point with @p clearance, within
/// `ACTOR_SNAP_RINGS`; nothing when @p point is off the grid or no cell
/// that near is open.
[[nodiscard]] std::optional<spatial::GridCell>
openCellNear(const spatial::NavGrid& grid, Vec2 point, uint8_t clearance);

/// The centre of `openCellNear` — or @p point itself on a grid with no
/// cells, where there is nothing to stand clear of.
[[nodiscard]] std::optional<Vec2> openPointNear(const spatial::NavGrid& grid,
                                                Vec2 point, uint8_t clearance);

}  // namespace eng::game
