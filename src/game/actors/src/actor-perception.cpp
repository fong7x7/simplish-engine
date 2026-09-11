#include "actor-passes.h"
#include "actor-queries.h"
#include "actor-tuning.h"

#include <algorithm>
#include <cmath>
#include <engine/input/input-action.h>
#include <engine/spatial/line-of-sight.h>
#include <optional>

namespace eng::game {

namespace {

  /// What an actor made of one candidate this tick.
  struct Sighting {
    /// Whom it is about.
    ActorCandidate who{};
    /// Whether the actor sees them.
    uint8_t seen = 0;
    /// Whether the actor hears them.
    uint8_t heard = 0;
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

  /// Whether @p who is a player who held fire this tick. Actors make no
  /// sound yet: nothing they do is loud.
  bool firing(const ActorTickContext& context, const ActorCandidate& who) {
    if (who.kind != ActorTargetKind::PLAYER) {
      return false;
    }
    const uint8_t slot = context.players.input_slot[who.index];
    return slot < context.input.players.size() &&
           (context.input.players[slot].buttons & input::INPUT_BUTTON_FIRE) !=
               0;
  }

  /// Where @p who stands, on the floor.
  Vec2 whereIs(const ActorRef& a, const ActorTickContext& context,
               const ActorCandidate& who) {
    return who.kind == ActorTargetKind::PLAYER
               ? flat(context.players.position[who.index])
               : flat(a.pool.position[who.index]);
  }

  /// What actor @p a makes of @p who.
  Sighting sight(const ActorRef& a, const ActorTickContext& context,
                 const ActorCandidate& who) {
    const Vec2 eye = flat(a.pool.position[a.i]);
    const Vec2 at = whereIs(a, context, who);
    const float d2 = who.distance_squared;
    const ActorBrain& brain = brainOf(a, context);
    const bool seen = d2 <= brain.sight_squared &&
                      inView(brain, a.pool.facing[a.i], at - eye, d2) &&
                      spatial::hasLineOfSight(context.grid, eye, at, 1);
    const bool heard = d2 <= brain.hearing_squared && firing(context, who);
    return {who, static_cast<uint8_t>(seen ? 1 : 0),
            static_cast<uint8_t>(heard ? 1 : 0)};
  }

  /// Whether @p x ranks before @p y as a target.
  bool ranksBefore(const ActorCandidate& x, const ActorCandidate& y) {
    if (x.current != y.current) {
      return x.current > y.current;
    }
    if (x.distance_squared != y.distance_squared) {
      return x.distance_squared < y.distance_squared;
    }
    return x.kind != y.kind ? x.kind < y.kind : x.index < y.index;
  }

  /// Whether actor @p a looks for targets among the players: a hostile one
  /// always, a friendly one unless it targets its opponents.
  bool targetsPlayers(const ActorRef& a, const ActorTickContext& context) {
    return a.pool.faction[a.i] == Faction::HOSTILE ||
           brainOf(a, context).behavior.senses.targets ==
               BehaviorTargets::PLAYERS;
  }

  /// Whether the actor at dense index @p j is an opponent of actor @p a.
  bool opposes(const ActorRef& a, uint32_t j) {
    const Faction mine = a.pool.faction[a.i];
    const Faction theirs = a.pool.faction[j];
    return (mine == Faction::HOSTILE && theirs == Faction::FRIENDLY) ||
           (mine == Faction::FRIENDLY && theirs == Faction::HOSTILE);
  }

  /// Every player within reach of actor @p a's senses, into @p out.
  void addPlayers(const ActorRef& a, const ActorTickContext& context,
                  float reach, std::vector<ActorCandidate>& out) {
    const Vec2 eye = flat(a.pool.position[a.i]);
    const bool mine = a.pool.target_kind[a.i] == ActorTargetKind::PLAYER;
    for (uint32_t p = 0; p < context.players.slots.size(); ++p) {
      const float d2 =
          Vec2::distanceSquared(eye, flat(context.players.position[p]));
      if (d2 <= reach) {
        const bool current =
            mine && context.players.slots.handleAt(p) == a.pool.target[a.i];
        out.push_back(
            {ActorTargetKind::PLAYER, p, static_cast<uint8_t>(current), d2});
      }
    }
  }

  /// Every opponent of actor @p a within its sight, into @p out, found by
  /// way of the neighbour grid rather than by asking every actor.
  void addOpponents(const ActorRef& a, const ActorTickContext& context,
                    const ActorWorkspace& workspace,
                    std::vector<ActorCandidate>& out) {
    const ActorBrain& brain = brainOf(a, context);
    const Vec2 eye = workspace.positions[a.i];
    const bool mine = a.pool.target_kind[a.i] == ActorTargetKind::ACTOR;
    workspace.neighbors.forEachNear(
        eye, brain.behavior.senses.sight_range, [&](uint32_t j) {
          const float d2 = Vec2::distanceSquared(eye, workspace.positions[j]);
          if (j != a.i && opposes(a, j) && d2 <= brain.sight_squared) {
            const bool current =
                mine && a.pool.slots.handleAt(j) == a.pool.target[a.i];
            out.push_back(
                {ActorTargetKind::ACTOR, j, static_cast<uint8_t>(current), d2});
          }
        });
  }

  /// Whom actor @p a might take as its target this tick, best first, into
  /// the workspace's candidate list.
  void rankCandidates(const ActorRef& a, const ActorTickContext& context,
                      ActorWorkspace& workspace) {
    std::vector<ActorCandidate>& list = workspace.candidates;
    list.clear();
    const ActorBrain& brain = brainOf(a, context);
    if (targetsPlayers(a, context)) {
      addPlayers(a, context,
                 std::max(brain.sight_squared, brain.hearing_squared), list);
    }
    if (brain.behavior.senses.targets == BehaviorTargets::OPPONENTS) {
      addOpponents(a, context, workspace, list);
    }
    std::ranges::sort(list, ranksBefore);
  }

  /// Whom actor @p a should take as its target this tick, if it perceives
  /// anyone: the best-ranked it perceives. Asked in rank order, so the walk
  /// for line of sight is made only until someone is seen or heard —
  /// usually once, rather than once for everyone in range.
  std::optional<Sighting> bestSighting(const ActorRef& a,
                                       const ActorTickContext& context,
                                       ActorWorkspace& workspace) {
    rankCandidates(a, context, workspace);
    for (const ActorCandidate& who : workspace.candidates) {
      const Sighting sighting = sight(a, context, who);
      if (sighting.seen != 0 || sighting.heard != 0) {
        return sighting;
      }
    }
    return std::nullopt;
  }

  /// Actor @p a forgets its target.
  void forget(const ActorRef& a) {
    a.pool.target[a.i] = {};
    a.pool.remembers_target[a.i] = 0;
  }

  /// Actor @p a takes note of whom it perceived.
  void remember(const ActorRef& a, const ActorTickContext& context,
                const Sighting& sighting) {
    const ActorCandidate& who = sighting.who;
    a.pool.target[a.i] = who.kind == ActorTargetKind::PLAYER
                             ? context.players.slots.handleAt(who.index)
                             : a.pool.slots.handleAt(who.index);
    a.pool.target_kind[a.i] = who.kind;
    a.pool.last_seen[a.i] = whereIs(a, context, who);
    a.pool.last_seen_tick[a.i] = context.tick;
    a.pool.remembers_target[a.i] = 1;
    a.pool.sees_target[a.i] = sighting.seen;
    a.pool.hears_target[a.i] = sighting.heard;
  }

  /// Whether actor @p a's target is still in the game.
  bool targetExists(const ActorRef& a, const ActorTickContext& context) {
    const sim::EntityHandle target = a.pool.target[a.i];
    return a.pool.target_kind[a.i] == ActorTargetKind::PLAYER
               ? context.players.slots.denseIndex(target).has_value()
               : a.pool.slots.denseIndex(target).has_value();
  }

  /// Whether actor @p a's memory of its target has lapsed: too long ago,
  /// or of someone no longer in the game.
  bool memoryLapsed(const ActorRef& a, const ActorTickContext& context) {
    const uint64_t since = context.tick - a.pool.last_seen_tick[a.i];
    return since > brainOf(a, context).behavior.senses.memory_ticks ||
           !targetExists(a, context);
  }

  /// Whether actor @p a perceives this tick. One near a player does every
  /// tick; one further off does on every `ACTOR_FAR_PERCEIVE_TICKS`th, on
  /// a tick its slot picks, so the far ones share the ticks between them
  /// (Game REQUIREMENTS §5.2's tiers, decided inside the simulation).
  bool perceivesNow(const ActorRef& a, const ActorTickContext& context) {
    const Vec2 at = flat(a.pool.position[a.i]);
    constexpr float NEAR = ACTOR_NEAR_TIER_TILES * ACTOR_NEAR_TIER_TILES;
    for (uint32_t p = 0; p < context.players.slots.size(); ++p) {
      if (Vec2::distanceSquared(at, flat(context.players.position[p])) <=
          NEAR) {
        return true;
      }
    }
    const uint32_t slot = a.pool.slots.handleAt(a.i).index;
    return (context.tick + slot) % ACTOR_FAR_PERCEIVE_TICKS == 0;
  }

}  // namespace

void perceiveActor(const ActorRef& a, const ActorTickContext& context,
                   ActorWorkspace& workspace) {
  if (!perceivesNow(a, context)) {
    return;
  }
  a.pool.sees_target[a.i] = 0;
  a.pool.hears_target[a.i] = 0;
  if (a.pool.faction[a.i] == Faction::NEUTRAL) {
    forget(a);
    return;
  }
  const std::optional<Sighting> best = bestSighting(a, context, workspace);
  if (best) {
    remember(a, context, *best);
  } else if (a.pool.remembers_target[a.i] != 0 && memoryLapsed(a, context)) {
    forget(a);
  }
}

}  // namespace eng::game
