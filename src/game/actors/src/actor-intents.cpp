#include "actor-passes.h"
#include "actor-queries.h"
#include "actor-tuning.h"

#include <array>
#include <cmath>
#include <cstddef>
#include <engine/math/sin-cos.h>

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

  /// Toward where the target was seen, stopping `near_tiles` short.
  void approach(const ActorRef& a,
                [[maybe_unused]] const ActorTickContext& context,
                const BehaviorState& state, ActorIntent& intent) {
    if (hasTarget(a)) {
      goTo(a, intent, a.pool.last_seen[a.i], state.near_tiles);
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

  /// Each action's intent, in enumerator order.
  constexpr std::array<IntentFn, 10> INTENTS{
      stand, stand,    wander, approach,   keepDistance,
      flee,  approach, search, returnHome, charge};
  static_assert(INTENTS.size() ==
                static_cast<size_t>(BehaviorAction::CHARGE) + 1);

}  // namespace

void intendActor(const ActorRef& a, const ActorTickContext& context,
                 ActorIntent& intent) {
  intent = {};
  const BehaviorState& state = stateOf(a, context);
  INTENTS[static_cast<size_t>(state.action)](a, context, state, intent);
}

}  // namespace eng::game
