#include "actor-passes.h"
#include "actor-queries.h"

#include <cmath>
#include <engine/input/input-action.h>
#include <engine/spatial/line-of-sight.h>
#include <optional>

namespace eng::game {

namespace {

  /// What an actor made of one player this tick.
  struct Sighting {
    /// The player's dense index.
    uint32_t player = 0;
    /// Whether the actor sees them.
    uint8_t seen = 0;
    /// Whether the actor hears them.
    uint8_t heard = 0;
    /// How far away they are, squared.
    float distance_squared = 0.0F;
  };

  /// Whether @p offset, @p distance_squared long, lies in @p brain's view
  /// cone about @p facing.
  bool inView(const ActorBrain& brain, Vec2 facing, Vec2 offset,
              float distance_squared) {
    if (brain.view_cos < -1.0F || distance_squared == 0.0F) {
      return true;
    }
    return Vec2::dot(facing, offset) >=
           brain.view_cos * std::sqrt(distance_squared);
  }

  /// Whether the player at dense index @p player held fire this tick.
  bool firing(const ActorTickContext& context, uint32_t player) {
    const uint8_t slot = context.players.input_slot[player];
    return slot < context.input.players.size() &&
           (context.input.players[slot].buttons & input::INPUT_BUTTON_FIRE) !=
               0;
  }

  /// What actor @p a makes of the player at dense index @p player.
  Sighting sight(const ActorRef& a, const ActorTickContext& context,
                 uint32_t player) {
    const Vec2 eye = flat(a.pool.position[a.i]);
    const Vec2 at = flat(context.players.position[player]);
    const Vec2 offset = at - eye;
    const float d2 = Vec2::lengthSquared(offset);
    const ActorBrain& brain = brainOf(a, context);
    const bool seen = d2 <= brain.sight_squared &&
                      inView(brain, a.pool.facing[a.i], offset, d2) &&
                      spatial::hasLineOfSight(context.grid, eye, at, 1);
    const bool heard = d2 <= brain.hearing_squared && firing(context, player);
    return {player, static_cast<uint8_t>(seen ? 1 : 0),
            static_cast<uint8_t>(heard ? 1 : 0), d2};
  }

  /// Whether @p candidate should replace @p best as the actor's target: the
  /// target it already has wins, and otherwise the nearer.
  bool prefer(const ActorRef& a, const ActorTickContext& context,
              const Sighting& candidate, const Sighting& best) {
    const sim::EntityHandle current = a.pool.target[a.i];
    const bool candidate_current =
        context.players.slots.handleAt(candidate.player) == current;
    const bool best_current =
        context.players.slots.handleAt(best.player) == current;
    if (candidate_current != best_current) {
      return candidate_current;
    }
    return candidate.distance_squared < best.distance_squared;
  }

  /// The player actor @p a should take as its target this tick, if it
  /// perceives any.
  std::optional<Sighting> bestSighting(const ActorRef& a,
                                       const ActorTickContext& context) {
    std::optional<Sighting> best;
    for (uint32_t p = 0; p < context.players.slots.size(); ++p) {
      const Sighting candidate = sight(a, context, p);
      if ((candidate.seen != 0 || candidate.heard != 0) &&
          (!best || prefer(a, context, candidate, *best))) {
        best = candidate;
      }
    }
    return best;
  }

  /// Actor @p a forgets its target.
  void forget(const ActorRef& a) {
    a.pool.target[a.i] = {};
    a.pool.remembers_target[a.i] = 0;
  }

  /// Actor @p a takes note of the player it perceived.
  void remember(const ActorRef& a, const ActorTickContext& context,
                const Sighting& sighting) {
    a.pool.target[a.i] = context.players.slots.handleAt(sighting.player);
    a.pool.last_seen[a.i] = flat(context.players.position[sighting.player]);
    a.pool.last_seen_tick[a.i] = context.tick;
    a.pool.remembers_target[a.i] = 1;
    a.pool.sees_target[a.i] = sighting.seen;
    a.pool.hears_target[a.i] = sighting.heard;
  }

  /// Whether actor @p a's memory of its target has lapsed: too long ago,
  /// or of a player no longer in the game.
  bool memoryLapsed(const ActorRef& a, const ActorTickContext& context) {
    const uint64_t since = context.tick - a.pool.last_seen_tick[a.i];
    return since > brainOf(a, context).behavior.senses.memory_ticks ||
           !context.players.slots.denseIndex(a.pool.target[a.i]).has_value();
  }

}  // namespace

void perceiveActor(const ActorRef& a, const ActorTickContext& context) {
  a.pool.sees_target[a.i] = 0;
  a.pool.hears_target[a.i] = 0;
  if (a.pool.faction[a.i] == Faction::NEUTRAL) {
    forget(a);
    return;
  }
  const std::optional<Sighting> best = bestSighting(a, context);
  if (best) {
    remember(a, context, *best);
  } else if (a.pool.remembers_target[a.i] != 0 && memoryLapsed(a, context)) {
    forget(a);
  }
}

}  // namespace eng::game
