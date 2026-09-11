#pragma once

/// @file actor-flow-fields.h
/// @brief Each player's flow field, and the one being rebuilt.
/// @par Threading
/// Main-thread-only; mutated only inside a tick.

#include <array>
#include <cstdint>
#include <engine/sim/state-hasher.h>
#include <engine/sim/tick-input.h>
#include <engine/spatial/flow-field-builder.h>
#include <engine/spatial/flow-field.h>
#include <engine/spatial/nav-grid.h>

namespace eng::game {

/// What `ActorFlowFields::building` holds when no field is being built.
inline constexpr uint8_t ACTOR_FLOW_IDLE = 0xFF;

/// Most cells the flow-field builder expands in one tick: a field over a
/// hand-authored level takes a few ticks, one over the largest grid a few
/// hundred, and no tick pays for more than this.
inline constexpr uint32_t ACTOR_FLOW_BUDGET_PER_TICK = 4096;

/// The shared flow fields a horde pursues players by (Game REQUIREMENTS
/// §5.2): one per player, toward the cell they stood in when it was built,
/// for an actor of the default radius. One is rebuilt at a time, a budget
/// of cells a tick, going round the players in slot order and skipping any
/// whose field already leads to the cell they stand in.
///
/// Simulation state: an actor's path depends on it, and what a field holds
/// depends on when it was built. It is hashed by what determines it — each
/// field's goal and whether it is complete, and how far the rebuild has
/// got — rather than cell by cell: a field is a pure function of the grid,
/// its goal and its clearance.
struct ActorFlowFields {
  /// Fields for no grid, which never build.
  ActorFlowFields() = default;
  /// Fields over @p grid for actors needing @p clearance.
  ActorFlowFields(const spatial::NavGrid& grid, uint8_t clearance);

  /// Each player's field, by input slot. Only a complete one is walked.
  std::array<spatial::FlowField, sim::MAX_PLAYERS> fields{};
  /// The field being rebuilt.
  spatial::FlowFieldBuilder builder;
  /// The clearance every field is built for.
  uint8_t clearance = 1;
  /// The input slot the builder is building for, or `ACTOR_FLOW_IDLE`.
  uint8_t building = ACTOR_FLOW_IDLE;
  /// The input slot the next rebuild considers first.
  uint8_t next = 0;
};

/// Fold @p flow into a tick hash section.
void hashFlowFields(const ActorFlowFields& flow, sim::StateHasher& hasher);

}  // namespace eng::game
