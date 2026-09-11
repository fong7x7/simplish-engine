#include "actor-conditions.h"

#include "actor-queries.h"

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

  /// Every condition's test, in enumerator order.
  constexpr std::array<ConditionTest, 12> TESTS{
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
  };
  static_assert(TESTS.size() ==
                static_cast<size_t>(BehaviorCondition::CHANCE) + 1);

}  // namespace

bool conditionHolds(const ActorRef& a, const ActorTickContext& context,
                    const BehaviorExit& exit) {
  const auto kind = static_cast<size_t>(exit.when);
  return kind < TESTS.size() && TESTS[kind](a, context, exit);
}

}  // namespace eng::game
