#pragma once

/// @file actor-pool.h
/// @brief Every actor in the simulation, as structure-of-arrays.
/// @par Threading
/// Main-thread-only; mutated only inside a tick.

#include <cstdint>
#include <engine/math/vec2.h>
#include <engine/math/vec3.h>
#include <engine/sim/entity-handle.h>
#include <engine/sim/entity-slots.h>
#include <game/actors/actor-path.h>
#include <game/actors/actor-target-kind.h>
#include <game/content/faction.h>
#include <vector>

namespace eng::game {

/// The actor pool (ADR-004): the enemies and NPCs, one array per field,
/// each indexed by dense index. Every field is simulation state and is
/// hashed; what a tick works out and throws away lives in `ActorWorkspace`.
///
/// Flags are bytes, 1 for true, so every array is a span of plain values a
/// hash takes as it is.
struct ActorPool {
  /// A pool with room for @p capacity actors.
  explicit ActorPool(uint32_t capacity = 0);

  /// Handles, dense indices, and deferred destruction.
  sim::EntitySlots slots;

  /// Where each actor's feet are, in tiles.
  std::vector<Vec3> position;
  /// The unit direction each actor faces, world X and Y.
  std::vector<Vec2> facing;
  /// Where each actor spawned: what `return_home` walks back to.
  std::vector<Vec2> home;
  /// Each actor's radius to collision, in tiles.
  std::vector<float> radius;
  /// Each actor's height to collision, in tiles.
  std::vector<float> height;
  /// Which of the world's brains each actor runs.
  std::vector<uint16_t> brain;
  /// Which side each actor is on.
  std::vector<Faction> faction;

  /// The state of its behavior each actor is in.
  std::vector<uint8_t> state;
  /// The tick each actor entered its state on.
  std::vector<uint64_t> state_since;
  /// Who each actor last perceived; null when it has none.
  std::vector<sim::EntityHandle> target;
  /// Which pool each actor's `target` is in.
  std::vector<ActorTargetKind> target_kind;
  /// Whether each actor sees its target this tick.
  std::vector<uint8_t> sees_target;
  /// Whether each actor hears its target this tick.
  std::vector<uint8_t> hears_target;
  /// Whether each actor still remembers a target.
  std::vector<uint8_t> remembers_target;
  /// Where each actor last perceived its target.
  std::vector<Vec2> last_seen;
  /// The tick each actor last perceived its target on.
  std::vector<uint64_t> last_seen_tick;

  /// Where each actor's action is taking it.
  std::vector<Vec2> goal;
  /// Whether each actor's `goal` was chosen and should be kept — a wander
  /// spot or a place to flee to — rather than worked out afresh each tick.
  std::vector<uint8_t> has_goal;
  /// Whether each actor reached its goal last tick, or had none to reach.
  std::vector<uint8_t> arrived;
  /// Whether each actor tried to move last tick and was stopped.
  std::vector<uint8_t> blocked;
  /// Whether each actor's last plan found no way to its goal.
  std::vector<uint8_t> no_path;
  /// The route each actor is following.
  std::vector<ActorPath> path;

  /// Which of the world's routes each actor patrols, or `ACTOR_NO_ROUTE`.
  std::vector<uint16_t> route;
  /// The waypoint of its route each actor is walking to.
  std::vector<uint16_t> route_leg;
  /// Whether each actor is walking its route backwards, turning at the
  /// ends, as a `ping_pong` patrol does.
  std::vector<uint8_t> route_reverse;
};

}  // namespace eng::game
