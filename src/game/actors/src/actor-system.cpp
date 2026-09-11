#include "actor-passes.h"
#include "actor-queries.h"

#include <algorithm>
#include <engine/math/sin-cos.h>
#include <engine/sim/slot-move.h>
#include <game/actors/actor-system.h>
#include <span>

namespace eng::game {

namespace {

  /// The smallest radius or height an actor is given: something to
  /// collide with and to pick, whatever a spawn says.
  constexpr float ACTOR_MIN_SIZE_TILES = 0.05F;

  /// Run @p pass on every live actor, in dense order.
  template <typename Pass> void eachActor(ActorPool& pool, Pass pass) {
    for (uint32_t i = 0; i < pool.slots.size(); ++i) {
      pass(ActorRef{pool, i});
    }
  }

  /// Set the body of the actor at dense index @p i from @p spawn.
  void placeBody(ActorPool& pool, uint32_t i, const ActorSpawn& spawn) {
    const math::SinCos yaw = math::sinCosDegrees(spawn.yaw_degrees);
    pool.position[i] = spawn.at;
    pool.facing[i] = {yaw.cos, yaw.sin};
    pool.home[i] = flat(spawn.at);
    pool.radius[i] = std::max(spawn.radius, ACTOR_MIN_SIZE_TILES);
    pool.height[i] = std::max(spawn.height, ACTOR_MIN_SIZE_TILES);
    pool.faction[i] = spawn.faction;
  }

  /// Start the mind of the actor at dense index @p i: brain @p brain, in
  /// state @p initial, knowing nothing.
  void resetMind(ActorPool& pool, uint32_t i, uint16_t brain, uint8_t initial) {
    pool.brain[i] = brain;
    pool.state[i] = initial;
    pool.state_since[i] = 0;
    pool.target[i] = {};
    pool.sees_target[i] = 0;
    pool.hears_target[i] = 0;
    pool.remembers_target[i] = 0;
    pool.last_seen[i] = pool.home[i];
    pool.last_seen_tick[i] = 0;
  }

  /// Start the movement of the actor at dense index @p i: at home, going
  /// nowhere.
  void resetMovement(ActorPool& pool, uint32_t i) {
    pool.goal[i] = pool.home[i];
    pool.has_goal[i] = 0;
    pool.arrived[i] = 0;
    pool.blocked[i] = 0;
    pool.no_path[i] = 0;
    pool.path[i] = {};
    pool.route[i] = ACTOR_NO_ROUTE;
    pool.route_leg[i] = 0;
    pool.route_reverse[i] = 0;
  }

  /// Apply @p moves to the body fields.
  void compactBody(ActorPool& pool, std::span<const sim::SlotMove> moves) {
    sim::applySlotMoves(moves, pool.position);
    sim::applySlotMoves(moves, pool.facing);
    sim::applySlotMoves(moves, pool.home);
    sim::applySlotMoves(moves, pool.radius);
    sim::applySlotMoves(moves, pool.height);
    sim::applySlotMoves(moves, pool.brain);
    sim::applySlotMoves(moves, pool.faction);
  }

  /// Apply @p moves to the mind fields.
  void compactMind(ActorPool& pool, std::span<const sim::SlotMove> moves) {
    sim::applySlotMoves(moves, pool.state);
    sim::applySlotMoves(moves, pool.state_since);
    sim::applySlotMoves(moves, pool.target);
    sim::applySlotMoves(moves, pool.sees_target);
    sim::applySlotMoves(moves, pool.hears_target);
    sim::applySlotMoves(moves, pool.remembers_target);
    sim::applySlotMoves(moves, pool.last_seen);
    sim::applySlotMoves(moves, pool.last_seen_tick);
  }

  /// Apply @p moves to the movement fields.
  void compactMovement(ActorPool& pool, std::span<const sim::SlotMove> moves) {
    sim::applySlotMoves(moves, pool.goal);
    sim::applySlotMoves(moves, pool.has_goal);
    sim::applySlotMoves(moves, pool.arrived);
    sim::applySlotMoves(moves, pool.blocked);
    sim::applySlotMoves(moves, pool.no_path);
    sim::applySlotMoves(moves, pool.path);
    sim::applySlotMoves(moves, pool.route);
    sim::applySlotMoves(moves, pool.route_leg);
    sim::applySlotMoves(moves, pool.route_reverse);
  }

  /// The first @p count entries of @p field: the live ones.
  template <typename T>
  std::span<const T> live(const std::vector<T>& field, uint32_t count) {
    return std::span<const T>(field).first(count);
  }

  /// Fold the body fields in.
  void hashBody(const ActorPool& pool, sim::StateHasher& hasher, uint32_t n) {
    hasher.addSpan(live(pool.position, n));
    hasher.addSpan(live(pool.facing, n));
    hasher.addSpan(live(pool.home, n));
    hasher.addSpan(live(pool.radius, n));
    hasher.addSpan(live(pool.height, n));
    hasher.addSpan(live(pool.brain, n));
    hasher.addSpan(live(pool.faction, n));
  }

  /// Fold the mind fields in.
  void hashMind(const ActorPool& pool, sim::StateHasher& hasher, uint32_t n) {
    hasher.addSpan(live(pool.state, n));
    hasher.addSpan(live(pool.state_since, n));
    hasher.addSpan(live(pool.target, n));
    hasher.addSpan(live(pool.sees_target, n));
    hasher.addSpan(live(pool.hears_target, n));
    hasher.addSpan(live(pool.remembers_target, n));
    hasher.addSpan(live(pool.last_seen, n));
    hasher.addSpan(live(pool.last_seen_tick, n));
  }

  /// Fold the movement fields in.
  void hashMovement(const ActorPool& pool, sim::StateHasher& hasher,
                    uint32_t n) {
    hasher.addSpan(live(pool.goal, n));
    hasher.addSpan(live(pool.has_goal, n));
    hasher.addSpan(live(pool.arrived, n));
    hasher.addSpan(live(pool.blocked, n));
    hasher.addSpan(live(pool.no_path, n));
    hasher.addSpan(live(pool.path, n));
    hasher.addSpan(live(pool.route, n));
    hasher.addSpan(live(pool.route_leg, n));
    hasher.addSpan(live(pool.route_reverse, n));
  }

}  // namespace

std::optional<sim::EntityHandle> spawnActor(ActorPool& pool,
                                            const ActorSpawn& spawn,
                                            uint16_t brain_index,
                                            const ActorBrain& brain) {
  const std::optional<sim::EntityHandle> handle = pool.slots.spawn();
  if (!handle) {
    return std::nullopt;
  }
  // `spawn` always places a new entity at the end of the dense range.
  const uint32_t i = pool.slots.size() - 1U;
  placeBody(pool, i, spawn);
  resetMind(pool, i, brain_index, brain.behavior.initial);
  resetMovement(pool, i);
  return handle;
}

void stepActors(ActorPool& pool, const ActorTickContext& context,
                ActorWorkspace& workspace) {
  workspace.path_budget = ACTOR_PATH_BUDGET_PER_TICK;
  std::span<ActorIntent> intents(workspace.intents);
  eachActor(pool, [&](const ActorRef& a) { perceiveActor(a, context); });
  eachActor(pool, [&](const ActorRef& a) { decideActor(a, context); });
  eachActor(pool,
            [&](const ActorRef& a) { intendActor(a, context, intents[a.i]); });
  eachActor(pool, [&](const ActorRef& a) { planActor(a, context, workspace); });
  eachActor(pool,
            [&](const ActorRef& a) { steerActor(a, context, intents[a.i]); });
  separateActors(pool, context.players, intents);
  eachActor(pool,
            [&](const ActorRef& a) { moveActor(a, context, intents[a.i]); });
  eachActor(pool,
            [&](const ActorRef& a) { faceActor(a, context, intents[a.i]); });
}

void compactActors(ActorPool& pool) {
  const auto moves = pool.slots.compact();
  compactBody(pool, moves);
  compactMind(pool, moves);
  compactMovement(pool, moves);
}

void hashActors(const ActorPool& pool, sim::StateHasher& hasher) {
  const uint32_t n = pool.slots.size();
  pool.slots.hashInto(hasher);
  hashBody(pool, hasher, n);
  hashMind(pool, hasher, n);
  hashMovement(pool, hasher, n);
}

}  // namespace eng::game
