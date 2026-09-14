// Pursuit of a target who has gone out of sight: knowing where they went,
// and a path that keeps up with them without being planned again.

#include "support/actor-arena.h"

#include <catch2/catch_test_macros.hpp>
#include <engine/input/input-action.h>
#include <game/content/behavior-lookup.h>
#include <vector>

using eng::Vec2;
using eng::game::ACTOR_PATH_BUDGET_PER_TICK;
using eng::game::BehaviorDefinition;
using eng::game::resolveBehavior;
using eng::game::test::ActorArena;

namespace {

/// A wall from y = -10 to 10 at x = 0, between the actors' side and the
/// players'.
const eng::physics::CollisionBox WALL{{0.0F, -10.0F, 0.0F},
                                      {0.5F, 10.0F, 2.0F}};

/// A pillar on the players' side of the wall, three tiles each way of
/// y = 0: what is just behind it is hidden from the wall's ends too.
const eng::physics::CollisionBox PILLAR{{7.0F, -3.0F, 0.0F},
                                        {7.5F, 3.0F, 2.0F}};

/// A pillar nine tiles each way of y = 0: what is well behind it is hidden
/// from the wall's ends.
const eng::physics::CollisionBox TALL_PILLAR{{7.0F, -9.0F, 0.0F},
                                             {7.5F, 9.0F, 2.0F}};

/// Whether @p v is exactly @p w.
bool same(Vec2 v, Vec2 w) {
  return v.x == w.x && v.y == w.y;
}

/// Where a player walking @p corners in order, five tiles a second from
/// the first, has got to on tick @p tick.
Vec2 walkedTo(const std::vector<Vec2>& corners, uint64_t tick) {
  float left = 5.0F / 60.0F * static_cast<float>(tick);
  for (size_t i = 1; i < corners.size(); ++i) {
    const Vec2 leg = corners[i] - corners[i - 1];
    const float length = Vec2::length(leg);
    if (left <= length) {
      return corners[i - 1] + leg * (left / length);
    }
    left -= length;
  }
  return corners.back();
}

/// Step @p arena @p ticks ticks, with its first player walking @p corners.
void stepWalking(ActorArena& arena, const std::vector<Vec2>& corners,
                 uint32_t ticks) {
  for (uint32_t t = 0; t < ticks; ++t) {
    arena.movePlayer(0, walkedTo(corners, arena.tick));
    arena.step();
  }
}

/// The chase preset rooted to the spot: it perceives and tracks, but
/// never moves, so what it knows is all a test sees change.
BehaviorDefinition rootedChase(uint32_t track_ticks) {
  BehaviorDefinition behavior = resolveBehavior({}, "chase");
  behavior.movement.speed = 0.0F;
  behavior.senses.track_ticks = track_ticks;
  return behavior;
}

/// A chase too wide for the players' flow fields, which plans its own
/// paths, and hears a shot anywhere in the arena — so it knows where its
/// quarry is behind the wall every tick.
struct WidePursuit {
  /// The arena: the wall and a pillar.
  ActorArena arena;
  /// The actor, on the far side of the wall.
  uint32_t actor = 0;

  /// The actor, two ticks into chasing a player five tiles beyond the wall,
  /// with @p pillar beyond them.
  explicit WidePursuit(const eng::physics::CollisionBox& pillar = PILLAR)
    : arena({WALL, pillar}) {
    BehaviorDefinition behavior = resolveBehavior({}, "chase");
    behavior.senses.hearing_range = 40.0F;
    arena.addPlayer({5.0F, 0.0F});
    arena.setFiring(0, eng::input::INPUT_BUTTON_FIRE);
    actor =
        arena.addActor(behavior, {.at = {-6.0F, 0.0F, 0.0F}, .radius = 0.9F});
    arena.step(2);
  }

  /// The path the actor is walking.
  [[nodiscard]] const eng::game::ActorPath& path() const {
    return arena.actors.path[actor];
  }
};

}  // namespace

TEST_CASE("an actor that loses sight of its target knows where they go for "
          "its tracking time, and no longer") {
  ActorArena arena({WALL});
  arena.addPlayer({-4.0F, 5.0F});
  const uint32_t actor =
      arena.addActor(rootedChase(30), {.at = {-4.0F, 0.0F, 0.0F}});
  arena.step();
  REQUIRE(arena.actors.sees_target[actor] == 1);

  arena.movePlayer(0, {4.0F, 0.0F});
  arena.step(20);
  REQUIRE(arena.actors.sees_target[actor] == 0);
  REQUIRE(same(arena.actors.last_seen[actor], {4.0F, 0.0F}));

  arena.step(20);
  arena.movePlayer(0, {4.0F, -3.0F});
  arena.step();
  REQUIRE(same(arena.actors.last_seen[actor], {4.0F, 0.0F}));
  REQUIRE(arena.actors.remembers_target[actor] == 1);
}

TEST_CASE("an actor with no tracking time goes to where it lost its target") {
  ActorArena arena({WALL});
  arena.addPlayer({-4.0F, 5.0F});
  const uint32_t actor =
      arena.addActor(rootedChase(0), {.at = {-4.0F, 0.0F, 0.0F}});
  arena.step();
  arena.movePlayer(0, {4.0F, 0.0F});
  arena.step(5);
  REQUIRE(same(arena.actors.last_seen[actor], {-4.0F, 5.0F}));
}

TEST_CASE("a chaser slower than its quarry follows them round two corners "
          "it never saw them turn") {
  // The player rounds the wall's foot, then a second wall, and stops out of
  // sight of everywhere the chaser saw them from.
  ActorArena arena({{{0.0F, -5.0F, 0.0F}, {0.5F, 20.0F, 2.0F}},
                    {{2.0F, -3.5F, 0.0F}, {8.0F, -3.0F, 2.0F}}});
  const std::vector<Vec2> corners{
      {-4.1F, 0.1F}, {-2.1F, -7.1F}, {9.1F, -7.1F}, {9.1F, 8.1F}, {3.1F, 8.1F}};
  arena.addPlayer(corners.front());
  BehaviorDefinition slow = resolveBehavior({}, "chase");
  slow.movement.speed = 2.5F;
  const uint32_t actor = arena.addActor(slow, {.at = {-12.0F, 0.0F, 0.0F}});

  stepWalking(arena, corners, 1500);
  REQUIRE(Vec2::distance(arena.actorAt(actor), corners.back()) < 1.5F);
  REQUIRE(arena.stateOf(actor) == "pursue");
}

TEST_CASE("a path whose goal moves where its end can walk straight is "
          "re-aimed without a search") {
  WidePursuit chase;
  REQUIRE(chase.path().count > 1);
  const uint64_t planned = chase.path().planned_tick;

  chase.arena.movePlayer(0, {5.0F, 1.5F});
  chase.arena.step();
  REQUIRE(chase.arena.workspace.path_budget == ACTOR_PATH_BUDGET_PER_TICK);
  REQUIRE(chase.path().planned_tick == planned);
  REQUIRE(chase.path().goal == chase.arena.grid.cellAt({5.0F, 1.5F}));
}

TEST_CASE("a path whose goal moves out of its end's sight is mended from its "
          "end") {
  WidePursuit chase;
  const uint64_t planned = chase.path().planned_tick;

  // Behind the pillar from everywhere the path goes.
  chase.arena.movePlayer(0, {9.0F, 0.0F});
  chase.arena.step();
  const uint32_t spent =
      ACTOR_PATH_BUDGET_PER_TICK - chase.arena.workspace.path_budget;
  REQUIRE(spent > 0);
  REQUIRE(spent <= 2048);
  const auto& path = chase.path();
  REQUIRE(path.planned_tick == planned);
  REQUIRE(path.goal == chase.arena.grid.cellAt({9.0F, 0.0F}));
  REQUIRE(Vec2::distance(path.points[path.count - 1], {9.0F, 0.0F}) < 0.25F);
}

TEST_CASE("a path mended for a second is planned again from where the actor "
          "stands") {
  WidePursuit chase;
  chase.arena.step(60);
  chase.arena.movePlayer(0, {9.0F, 0.0F});
  chase.arena.step();
  REQUIRE(chase.path().planned_tick == chase.arena.tick - 1);
}

TEST_CASE("a goal that moves far out of the path's sight is planned again "
          "from where the actor stands") {
  WidePursuit chase(TALL_PILLAR);
  // Past the quarter second a moved goal waits before a plan.
  chase.arena.step(15);
  chase.arena.movePlayer(0, {14.0F, 0.0F});
  const uint64_t moved = chase.arena.tick;
  // So far from the player, the actor perceives on every sixth tick.
  chase.arena.step(6);
  REQUIRE(chase.path().planned_tick >= moved);
}
