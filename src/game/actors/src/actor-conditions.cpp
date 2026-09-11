#include "actor-conditions.h"

#include "actor-queries.h"

#include <algorithm>
#include <array>
#include <cstddef>

namespace eng::game {

namespace {

  /// A condition's test.
  using ConditionTest = bool (*)(const ActorRef&, const ActorTickContext&,
                                 const BehaviorExit&);

  /// Squared distance from actor @p a to where its target was seen.
  float targetDistanceSquared(const ActorRef& a) {
    return Vec2::distanceSquared(flat(a.pool.position[a.i]),
                                 a.pool.last_seen[a.i]);
  }

  /// Whether actor @p a has gone @p ticks without perceiving its target.
  bool lostFor(const ActorRef& a, uint64_t now, uint32_t ticks) {
    if (a.pool.sees_target[a.i] != 0 || a.pool.hears_target[a.i] != 0) {
      return false;
    }
    return a.pool.remembers_target[a.i] == 0 ||
           now - a.pool.last_seen_tick[a.i] >= ticks;
  }

  /// Whether actor @p a was hurt within the last @p ticks ticks, at least
  /// one.
  bool hurtWithin(const ActorRef& a, uint64_t now, uint32_t ticks) {
    const uint64_t when = a.pool.damaged_tick[a.i];
    return when != ACTOR_NEVER_DAMAGED &&
           now - when <= std::max<uint64_t>(ticks, 1);
  }

  /// Whether actor @p a's health is below @p permille of its full health.
  bool healthBelow(const ActorRef& a, uint16_t permille) {
    return static_cast<uint32_t>(a.pool.health[a.i]) * 1000U <
           static_cast<uint32_t>(permille) * a.pool.max_health[a.i];
  }

  /// Whether another actor on actor @p a's side stands within @p tiles of
  /// it, found by way of the neighbour grid. Neutral actors have no side.
  bool alliesWithin(const ActorRef& a, const ActorWorkspace& workspace,
                    float tiles) {
    const Faction side = a.pool.faction[a.i];
    const Vec2 at = workspace.positions[a.i];
    bool found = false;
    workspace.neighbors.forEachNear(at, tiles, [&](uint32_t j) {
      found =
          found ||
          (j != a.i && a.pool.faction[j] == side && side != Faction::NEUTRAL &&
           Vec2::distanceSquared(at, workspace.positions[j]) <= tiles * tiles);
    });
    return found;
  }

  /// Every condition's test but `allies_within`, in enumerator order.
  constexpr std::array<ConditionTest, 14> TESTS{
      [](const ActorRef&, const ActorTickContext&, const BehaviorExit&) {
        return true;
      },
      [](const ActorRef& a, const ActorTickContext&, const BehaviorExit&) {
        return a.pool.sees_target[a.i] != 0;
      },
      [](const ActorRef& a, const ActorTickContext&, const BehaviorExit&) {
        return a.pool.hears_target[a.i] != 0;
      },
      [](const ActorRef& a, const ActorTickContext& c, const BehaviorExit& e) {
        return lostFor(a, c.tick, e.ticks);
      },
      [](const ActorRef& a, const ActorTickContext&, const BehaviorExit& e) {
        return a.pool.remembers_target[a.i] != 0 &&
               targetDistanceSquared(a) <= e.tiles * e.tiles;
      },
      [](const ActorRef& a, const ActorTickContext&, const BehaviorExit& e) {
        return a.pool.remembers_target[a.i] != 0 &&
               targetDistanceSquared(a) > e.tiles * e.tiles;
      },
      [](const ActorRef& a, const ActorTickContext& c, const BehaviorExit& e) {
        return c.tick - a.pool.state_since[a.i] >= e.ticks;
      },
      [](const ActorRef& a, const ActorTickContext&, const BehaviorExit&) {
        return a.pool.arrived[a.i] != 0;
      },
      [](const ActorRef& a, const ActorTickContext&, const BehaviorExit&) {
        return a.pool.no_path[a.i] != 0;
      },
      [](const ActorRef& a, const ActorTickContext&, const BehaviorExit&) {
        return a.pool.blocked[a.i] != 0;
      },
      [](const ActorRef& a, const ActorTickContext&, const BehaviorExit& e) {
        return Vec2::distanceSquared(flat(a.pool.position[a.i]),
                                     a.pool.home[a.i]) > e.tiles * e.tiles;
      },
      [](const ActorRef&, const ActorTickContext& c, const BehaviorExit& e) {
        return c.rng.nextBelow(1000) < e.permille;
      },
      [](const ActorRef& a, const ActorTickContext& c, const BehaviorExit& e) {
        return hurtWithin(a, c.tick, e.ticks);
      },
      [](const ActorRef& a, const ActorTickContext&, const BehaviorExit& e) {
        return healthBelow(a, e.permille);
      },
  };
  static_assert(TESTS.size() ==
                static_cast<size_t>(BehaviorCondition::ALLIES_WITHIN));

}  // namespace

bool conditionHolds(const ActorRef& a, const ActorTickContext& context,
                    const ActorWorkspace& workspace, const BehaviorExit& exit) {
  if (exit.when == BehaviorCondition::ALLIES_WITHIN) {
    return alliesWithin(a, workspace, exit.tiles);
  }
  const auto kind = static_cast<size_t>(exit.when);
  return kind < TESTS.size() && TESTS[kind](a, context, exit);
}

}  // namespace eng::game
