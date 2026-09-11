#include "actor-passes.h"
#include "actor-queries.h"

#include <game/actors/actor-flow-fields.h>
#include <utility>

namespace eng::game {

namespace {

  /// Fold one field's goal into @p hasher, with whether it is final.
  void hashField(const spatial::FlowField& field, sim::StateHasher& hasher) {
    hasher.add(static_cast<uint8_t>(field.complete() ? 1 : 0));
    hasher.add(field.goal().x);
    hasher.add(field.goal().y);
    hasher.add(field.clearance());
  }

  /// The dense index of the player in input slot @p slot, if one is.
  std::optional<uint32_t> playerInSlot(const PlayerPool& players,
                                       uint8_t slot) {
    for (uint32_t p = 0; p < players.slots.size(); ++p) {
      if (players.input_slot[p] == slot) {
        return p;
      }
    }
    return std::nullopt;
  }

  /// The cell a field for the player in @p slot should lead to, when its
  /// field does not lead there already.
  std::optional<spatial::GridCell> staleGoal(const ActorTickContext& context,
                                             uint8_t slot) {
    const ActorFlowFields& flow = context.flow;
    const auto player = playerInSlot(context.players, slot);
    if (!player) {
      return std::nullopt;
    }
    const auto goal = openCellNear(
        context.grid, flat(context.players.position[*player]), flow.clearance);
    const spatial::FlowField& field = flow.fields[slot];
    if (!goal || (field.complete() && field.goal() == *goal)) {
      return std::nullopt;
    }
    return goal;
  }

  /// Start rebuilding the field of the first player, from `next` round,
  /// whose field is stale. Nothing when every one is up to date.
  void startNextField(const ActorTickContext& context) {
    ActorFlowFields& flow = context.flow;
    for (uint8_t k = 0; k < sim::MAX_PLAYERS; ++k) {
      const auto slot =
          static_cast<uint8_t>((flow.next + k) % sim::MAX_PLAYERS);
      if (const auto goal = staleGoal(context, slot)) {
        flow.builder.start(context.grid, *goal, flow.clearance);
        flow.building = slot;
        flow.next = static_cast<uint8_t>((slot + 1U) % sim::MAX_PLAYERS);
        return;
      }
    }
  }

}  // namespace

ActorFlowFields::ActorFlowFields(const spatial::NavGrid& grid,
                                 uint8_t clearance_needed)
  : builder(grid.cellCount()), clearance(clearance_needed) {}

void hashFlowFields(const ActorFlowFields& flow, sim::StateHasher& hasher) {
  for (const spatial::FlowField& field : flow.fields) {
    hashField(field, hasher);
  }
  hasher.add(flow.building);
  hasher.add(flow.next);
  hasher.add(flow.builder.expanded());
  hashField(flow.builder.field(), hasher);
}

void advanceFlowFields(const ActorTickContext& context) {
  ActorFlowFields& flow = context.flow;
  if (context.grid.cellCount() == 0) {
    return;
  }
  if (flow.building == ACTOR_FLOW_IDLE) {
    startNextField(context);
  }
  if (flow.building != ACTOR_FLOW_IDLE &&
      flow.builder.advance(context.grid, ACTOR_FLOW_BUDGET_PER_TICK)) {
    std::swap(flow.fields[flow.building], flow.builder.field());
    flow.building = ACTOR_FLOW_IDLE;
  }
}

}  // namespace eng::game
