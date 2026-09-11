#include "actor-passes.h"
#include "actor-queries.h"
#include "actor-tuning.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <engine/math/sin-cos.h>
#include <game/actors/actor-route.h>
#include <game/player/player-system.h>

namespace eng::game {

namespace {

  /// One action's way of setting an actor's intent.
  using IntentFn = void (*)(const ActorRef&, const ActorTickContext&,
                            const BehaviorState&, ActorIntent&);

  /// Head actor @p a for @p point, arriving within @p stop tiles.
  void goTo(const ActorRef& a, ActorIntent& intent, Vec2 point, float stop) {
    a.pool.goal[a.i] = point;
    intent.moves = 1;
    intent.stop_within = stop;
  }

  /// Whether actor @p a has a target to act on.
  bool hasTarget(const ActorRef& a) {
    return a.pool.remembers_target[a.i] != 0;
  }

  /// A point @p distance tiles from actor @p a directly away from where its
  /// target was seen — or behind it, if the two are in the same place.
  Vec2 awayFromTarget(const ActorRef& a, float distance) {
    const Vec2 at = flat(a.pool.position[a.i]);
    Vec2 away = at - a.pool.last_seen[a.i];
    if (Vec2::lengthSquared(away) == 0.0F) {
      away = -a.pool.facing[a.i];
    }
    return at + Vec2::normalize(away) * distance;
  }

  /// Keep @p point as actor @p a's goal, moved to the nearest spot it can
  /// stand — or its own position, when there is none.
  void chooseGoal(const ActorRef& a, const ActorTickContext& context,
                  Vec2 point) {
    const Vec2 at = flat(a.pool.position[a.i]);
    a.pool.goal[a.i] =
        openPointNear(context.grid, point, clearanceOf(a, context.grid))
            .value_or(at);
    a.pool.has_goal[a.i] = 1;
  }

  /// A random spot within @p radius tiles of actor @p a's home.
  Vec2 wanderSpot(const ActorRef& a, const ActorTickContext& context,
                  float radius) {
    const math::SinCos way =
        math::sinCosDegrees(static_cast<float>(context.rng.nextBelow(360)));
    const float reach = radius * context.rng.nextUnitFloat();
    const Vec2 home = a.pool.home[a.i];
    return {home.x + way.cos * reach, home.y + way.sin * reach};
  }

  /// Nothing: stand where it is.
  void stand([[maybe_unused]] const ActorRef& a,
             [[maybe_unused]] const ActorTickContext& context,
             [[maybe_unused]] const BehaviorState& state,
             [[maybe_unused]] ActorIntent& intent) {}

  /// How near actor @p a comes to its target before it is touching them:
  /// both radii when the target is another actor, who yields to being
  /// pushed as a player does not — stopping any closer would shove them
  /// along ahead of it. Nothing for a player.
  float touching(const ActorRef& a) {
    if (a.pool.target_kind[a.i] != CombatantKind::ACTOR) {
      return 0.0F;
    }
    const auto other = a.pool.slots.denseIndex(a.pool.target[a.i]);
    return other ? a.pool.radius[a.i] + a.pool.radius[*other] : 0.0F;
  }

  /// Toward where the target was seen, stopping `near_tiles` short — or
  /// where it would touch them, if that is further.
  void approach(const ActorRef& a,
                [[maybe_unused]] const ActorTickContext& context,
                const BehaviorState& state, ActorIntent& intent) {
    if (hasTarget(a)) {
      goTo(a, intent, a.pool.last_seen[a.i],
           std::max(state.near_tiles, touching(a)));
      intent.chases = a.pool.sees_target[a.i] | a.pool.hears_target[a.i];
    }
  }

  /// How wide actor @p a's target is: a player's radius, another actor's
  /// own, or nothing when it has no target still in the game.
  float targetRadius(const ActorRef& a) {
    if (a.pool.target_kind[a.i] == CombatantKind::PLAYER) {
      return PLAYER_RADIUS_TILES;
    }
    const auto other = a.pool.slots.denseIndex(a.pool.target[a.i]);
    return other ? a.pool.radius[*other] : 0.0F;
  }

  /// Toward where the target was seen, to touching them — or `near_tiles`
  /// short, if that is further: a melee's approach.
  void closeIn(const ActorRef& a,
               [[maybe_unused]] const ActorTickContext& context,
               const BehaviorState& state, ActorIntent& intent) {
    if (hasTarget(a)) {
      const float touch = a.pool.radius[a.i] + targetRadius(a);
      goTo(a, intent, a.pool.last_seen[a.i], std::max(state.near_tiles, touch));
      intent.chases = a.pool.sees_target[a.i] | a.pool.hears_target[a.i];
    }
  }

  /// To where the target was last seen.
  void search(const ActorRef& a,
              [[maybe_unused]] const ActorTickContext& context,
              [[maybe_unused]] const BehaviorState& state,
              ActorIntent& intent) {
    if (hasTarget(a)) {
      goTo(a, intent, a.pool.last_seen[a.i], ACTOR_ARRIVE_TILES);
    }
  }

  /// Back to where it spawned.
  void returnHome(const ActorRef& a,
                  [[maybe_unused]] const ActorTickContext& context,
                  [[maybe_unused]] const BehaviorState& state,
                  ActorIntent& intent) {
    goTo(a, intent, a.pool.home[a.i], ACTOR_ARRIVE_TILES);
  }

  /// To a random spot near home, and on to another after a pause there.
  void wander(const ActorRef& a, const ActorTickContext& context,
              const BehaviorState& state, ActorIntent& intent) {
    if (a.pool.has_goal[a.i] == 0 ||
        (a.pool.arrived[a.i] != 0 &&
         context.rng.nextBelow(1000) < ACTOR_WANDER_REPICK_PERMILLE)) {
      chooseGoal(a, context, wanderSpot(a, context, state.far_tiles));
    }
    goTo(a, intent, a.pool.goal[a.i], ACTOR_ARRIVE_TILES);
  }

  /// Whether actor @p a's flight needs a new destination: it has none, got
  /// there, ran into something, or its target is now nearer the
  /// destination than half a flight.
  bool fleeGoalStale(const ActorRef& a, float distance) {
    const float half = distance * 0.5F;
    return a.pool.has_goal[a.i] == 0 || a.pool.arrived[a.i] != 0 ||
           a.pool.blocked[a.i] != 0 ||
           Vec2::distanceSquared(a.pool.goal[a.i], a.pool.last_seen[a.i]) <
               half * half;
  }

  /// Away from where the target was seen, `far_tiles` at a time.
  void flee(const ActorRef& a, const ActorTickContext& context,
            const BehaviorState& state, ActorIntent& intent) {
    if (!hasTarget(a)) {
      return;
    }
    if (fleeGoalStale(a, state.far_tiles)) {
      chooseGoal(a, context, awayFromTarget(a, state.far_tiles));
    }
    goTo(a, intent, a.pool.goal[a.i], ACTOR_ARRIVE_TILES);
  }

  /// Between `near_tiles` and `far_tiles` of the target: back off inside
  /// the band's near edge, close in past its far edge, stand within it.
  void keepDistance(const ActorRef& a, const ActorTickContext& context,
                    const BehaviorState& state, ActorIntent& intent) {
    if (!hasTarget(a)) {
      return;
    }
    const float d =
        Vec2::distance(flat(a.pool.position[a.i]), a.pool.last_seen[a.i]);
    if (d < state.near_tiles) {
      chooseGoal(a, context, awayFromTarget(a, state.near_tiles - d + 1.0F));
      goTo(a, intent, a.pool.goal[a.i], ACTOR_ARRIVE_TILES);
    } else if (d > state.far_tiles) {
      goTo(a, intent, a.pool.last_seen[a.i],
           std::max(state.far_tiles - 0.5F, state.near_tiles));
    }
  }

  /// Straight ahead: steering moves it along its facing.
  void charge([[maybe_unused]] const ActorRef& a,
              [[maybe_unused]] const ActorTickContext& context,
              [[maybe_unused]] const BehaviorState& state,
              ActorIntent& intent) {
    intent.moves = 1;
  }

  /// The route actor @p a patrols, or nothing when it has none worth
  /// walking.
  const ActorRoute* routeOf(const ActorRef& a,
                            const ActorTickContext& context) {
    const uint16_t route = a.pool.route[a.i];
    if (route >= context.routes.size() ||
        context.routes[route].points.empty()) {
      return nullptr;
    }
    return &context.routes[route];
  }

  /// The leg after @p leg of a route of @p count waypoints, walked as
  /// @p mode says, turning @p reverse round at an end of a beat.
  uint16_t nextLeg(uint16_t leg, size_t count, BehaviorRouteMode mode,
                   uint8_t& reverse) {
    const auto last = static_cast<uint16_t>(count - 1);
    if (mode == BehaviorRouteMode::LOOP) {
      return leg >= last ? 0 : static_cast<uint16_t>(leg + 1);
    }
    if ((reverse != 0 && leg == 0) || (reverse == 0 && leg >= last)) {
      reverse = reverse != 0 ? 0 : 1;
    }
    return reverse != 0 ? static_cast<uint16_t>(leg - 1)
                        : static_cast<uint16_t>(leg + 1);
  }

  /// Whether actor @p a stands at @p point, as near as arriving counts.
  bool standsAt(const ActorRef& a, Vec2 point) {
    constexpr float THERE = ACTOR_ARRIVE_TILES + ACTOR_MIN_STEP_TILES;
    return Vec2::distanceSquared(flat(a.pool.position[a.i]), point) <=
           THERE * THERE;
  }

  /// Waypoint by waypoint round its route, going on to the next whenever it
  /// stands at the one it was walking to. Judged by where it stands rather
  /// than by what it last did, so a patrol that paused on reaching a
  /// waypoint moves on from it when it comes back, instead of walking to
  /// it again. A route of one waypoint is a post it keeps returning to.
  void patrol(const ActorRef& a, const ActorTickContext& context,
              const BehaviorState& state, ActorIntent& intent) {
    const ActorRoute* route = routeOf(a, context);
    if (route == nullptr) {
      return;
    }
    const size_t count = route->points.size();
    uint16_t& leg = a.pool.route_leg[a.i];
    leg = static_cast<uint16_t>(leg % count);
    if (count > 1 && standsAt(a, route->points[leg])) {
      leg = nextLeg(leg, count, state.route, a.pool.route_reverse[a.i]);
    }
    a.pool.has_goal[a.i] = 1;
    goTo(a, intent, route->points[leg], ACTOR_ARRIVE_TILES);
  }

  /// Each action's intent, in enumerator order. Firing, spitting and
  /// bursting are done standing; the attack pass does the rest.
  constexpr std::array<IntentFn, 15> INTENTS{
      stand,  stand,    wander, approach,   keepDistance,
      flee,   approach, search, returnHome, charge,
      patrol, closeIn,  stand,  stand,      stand};
  static_assert(INTENTS.size() ==
                static_cast<size_t>(BehaviorAction::DETONATE) + 1);

}  // namespace

void intendActor(const ActorRef& a, const ActorTickContext& context,
                 ActorIntent& intent) {
  intent = {};
  const BehaviorState& state = stateOf(a, context);
  INTENTS[static_cast<size_t>(state.action)](a, context, state, intent);
}

}  // namespace eng::game
