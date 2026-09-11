#include "actor-passes.h"
#include "actor-queries.h"
#include "actor-tuning.h"

#include <algorithm>
#include <cmath>
#include <game/player/player-system.h>

namespace eng::game {

namespace {

  /// The waypoint actor @p a heads for — past any it has reached — or its
  /// goal once its path has none left.
  Vec2 nextWaypoint(const ActorRef& a) {
    ActorPath& path = a.pool.path[a.i];
    const Vec2 at = flat(a.pool.position[a.i]);
    constexpr float REACHED = ACTOR_WAYPOINT_TILES * ACTOR_WAYPOINT_TILES;
    while (path.next < path.count &&
           Vec2::distanceSquared(at, path.points[path.next]) <= REACHED) {
      ++path.next;
    }
    return path.next < path.count ? path.points[path.next] : a.pool.goal[a.i];
  }

  /// How far actor @p a moves in a tick in its state.
  float speedOf(const ActorRef& a, const ActorTickContext& context) {
    return brainOf(a, context).speed_per_tick *
           static_cast<float>(stateOf(a, context).speed_permille) /
           static_cast<float>(BEHAVIOR_FULL_SPEED_PERMILLE);
  }

  /// How far @p a must move to be @p min_distance from @p b, as a push
  /// away from it: nothing when they are already that far apart. Two in
  /// the same spot are parted along X, the lower index going left.
  Vec2 pushApart(Vec2 a, Vec2 b, float min_distance, float side) {
    const Vec2 away = a - b;
    const float d2 = Vec2::lengthSquared(away);
    if (d2 >= min_distance * min_distance) {
      return {};
    }
    if (d2 == 0.0F) {
      return {side * min_distance, 0.0F};
    }
    const float d = std::sqrt(d2);
    return away * ((min_distance - d) / d);
  }

  /// The push that eases actor @p i out of every other actor — half the
  /// overlap each, since the other is pushed too — and wholly out of
  /// every player, who does not yield.
  Vec2 separation(const ActorPool& pool, const PlayerPool& players,
                  uint32_t i) {
    const Vec2 at = flat(pool.position[i]);
    Vec2 push{};
    for (uint32_t j = 0; j < pool.slots.size(); ++j) {
      if (j != i) {
        const float side = i < j ? -0.5F : 0.5F;
        push = push + pushApart(at, flat(pool.position[j]),
                                pool.radius[i] + pool.radius[j], side) *
                          0.5F;
      }
    }
    for (uint32_t p = 0; p < players.slots.size(); ++p) {
      push = push + pushApart(at, flat(players.position[p]),
                              pool.radius[i] + PLAYER_RADIUS_TILES, -1.0F);
    }
    return push;
  }

  /// Step actor @p a at up to @p speed toward its goal, by way of its
  /// path, stopping short where the intent says; or note it has arrived.
  void steerToGoal(const ActorRef& a, ActorIntent& intent, float speed) {
    const Vec2 at = flat(a.pool.position[a.i]);
    const float to_goal = Vec2::distance(at, a.pool.goal[a.i]);
    // A step that stops exactly at the stopping distance can land a rounding
    // past it; within the smallest step of it is there.
    a.pool.arrived[a.i] =
        to_goal <= intent.stop_within + ACTOR_MIN_STEP_TILES ? 1 : 0;
    if (a.pool.arrived[a.i] != 0) {
      return;
    }
    const Vec2 aim = nextWaypoint(a);
    const float reach =
        std::min(Vec2::distance(at, aim), to_goal - intent.stop_within);
    intent.step = Vec2::normalize(aim - at) * std::min(speed, reach);
  }

}  // namespace

void steerActor(const ActorRef& a, const ActorTickContext& context,
                ActorIntent& intent) {
  if (intent.moves == 0) {
    a.pool.arrived[a.i] = 1;
  } else if (stateOf(a, context).action == BehaviorAction::CHARGE) {
    intent.step = a.pool.facing[a.i] * speedOf(a, context);
  } else {
    steerToGoal(a, intent, speedOf(a, context));
  }
}

void separateActors(const ActorPool& pool, const PlayerPool& players,
                    std::span<ActorIntent> intents) {
  for (uint32_t i = 0; i < pool.slots.size(); ++i) {
    intents[i].step = intents[i].step + separation(pool, players, i);
  }
}

}  // namespace eng::game
