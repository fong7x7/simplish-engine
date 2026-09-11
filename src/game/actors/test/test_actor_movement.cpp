#include "support/actor-arena.h"

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>
#include <engine/input/input-action.h>
#include <engine/physics/cylinder-collision.h>
#include <game/content/behavior-lookup.h>

using Catch::Approx;
using eng::Vec2;
using eng::game::BehaviorAction;
using eng::game::BehaviorDefinition;
using eng::game::defaultBehaviorState;
using eng::game::resolveBehavior;
using eng::game::test::ActorArena;

namespace {

/// A behavior of one state doing @p action, seeing all round.
BehaviorDefinition only(BehaviorAction action) {
  BehaviorDefinition behavior;
  behavior.id = "only";
  behavior.states.push_back(defaultBehaviorState(action));
  behavior.states.back().id = "only";
  behavior.senses.view_degrees = 360.0F;
  return behavior;
}

/// A wall across x = 2 to 2.5, from y = -4 to 4.
const std::vector<eng::physics::CollisionBox> WALL{
    {{2.0F, -4.0F, 0.0F}, {2.5F, 4.0F, 2.0F}}};

/// Distance from actor @p actor to @p point.
float distanceTo(const ActorArena& arena, uint32_t actor, Vec2 point) {
  return Vec2::distance(arena.actorAt(actor), point);
}

}  // namespace

TEST_CASE("a pursuer walks up to a player it sees and stops short") {
  ActorArena arena;
  arena.addPlayer({6.0F, 0.0F});
  const uint32_t actor = arena.addActor(only(BehaviorAction::PURSUE), {});
  arena.step(200);

  const float gap = distanceTo(arena, actor, {6.0F, 0.0F});
  REQUIRE(gap <= 0.9F + 1e-4F);
  REQUIRE(gap >= 0.6F);
  REQUIRE(arena.actors.arrived[actor] == 1);
}

TEST_CASE("a pursuer goes round a wall, never into it") {
  ActorArena arena(WALL);
  const uint32_t player = arena.addPlayer({5.0F, 0.0F});
  arena.setFiring(player, eng::input::INPUT_BUTTON_FIRE);
  BehaviorDefinition hunter = only(BehaviorAction::PURSUE);
  hunter.senses.hearing_range = 8.0F;
  const uint32_t actor = arena.addActor(hunter, {});

  bool clipped = false;
  for (int tick = 0; tick < 900; ++tick) {
    arena.step();
    const Vec2 at = arena.actorAt(actor);
    clipped = clipped || eng::physics::cylinderOverlapsBox(
                             {at, 0.25F, 0.0F, 1.5F}, WALL[0]);
  }
  REQUIRE_FALSE(clipped);
  REQUIRE(distanceTo(arena, actor, {5.0F, 0.0F}) < 1.0F);
}

TEST_CASE("an actor turns toward where it walks, a little each tick") {
  ActorArena arena;
  arena.addPlayer({0.0F, 6.0F});
  BehaviorDefinition hunter = only(BehaviorAction::PURSUE);
  hunter.movement.turn_degrees_per_second = 60.0F;
  const uint32_t actor = arena.addActor(hunter, {});
  arena.step();
  // One degree a tick, from +X toward +Y.
  REQUIRE(arena.actors.facing[actor].y == Approx(0.0174524F));
  arena.step(200);
  REQUIRE(arena.actors.facing[actor].y == Approx(1.0F));
}

TEST_CASE("a charge runs straight ahead until a wall stops it") {
  ActorArena arena(WALL);
  BehaviorDefinition rush = only(BehaviorAction::CHARGE);
  rush.states[0].facing = eng::game::BehaviorFacing::LOCKED;
  const uint32_t actor = arena.addActor(rush, {.at = {0.0F, 1.0F, 0.0F}});
  arena.step(120);

  REQUIRE(arena.actorAt(actor).y == Approx(1.0F));
  REQUIRE(arena.actorAt(actor).x == Approx(2.0F - 0.3F).margin(1e-3));
  REQUIRE(arena.actors.blocked[actor] == 1);
}

TEST_CASE("a charger winds up, rushes past a dodging player, and recovers") {
  ActorArena arena;
  const uint32_t player = arena.addPlayer({3.0F, 0.0F});
  const uint32_t actor = arena.addActor(resolveBehavior({}, "charger"), {});
  arena.step(2);
  REQUIRE(arena.stateOf(actor) == "windup");
  arena.step(30);
  REQUIRE(arena.stateOf(actor) == "rush");
  arena.movePlayer(player, {3.0F, 2.5F});
  arena.step(24);
  REQUIRE(arena.stateOf(actor) == "recover");
  REQUIRE(arena.actorAt(actor).x > 3.0F);
  REQUIRE(arena.actorAt(actor).y == Approx(0.0F).margin(1e-4));
}

TEST_CASE("a charger that rushes into a player is stopped by them") {
  ActorArena arena;
  arena.addPlayer({3.0F, 0.0F});
  const uint32_t actor = arena.addActor(resolveBehavior({}, "charger"), {});
  arena.step(60);
  REQUIRE(arena.stateOf(actor) == "recover");
  REQUIRE(arena.actorAt(actor).x < 3.0F - 0.59F);
}

TEST_CASE("actors in one spot are eased apart") {
  ActorArena arena;
  const uint32_t a = arena.addActor(only(BehaviorAction::HOLD), {});
  const uint32_t b = arena.addActor(only(BehaviorAction::HOLD), {});
  arena.step(60);
  REQUIRE(Vec2::distance(arena.actorAt(a), arena.actorAt(b)) >= 0.59F);
}

TEST_CASE("an actor walks home and arrives") {
  ActorArena arena(WALL);
  const uint32_t actor = arena.addActor(only(BehaviorAction::RETURN_HOME),
                                        {.at = {0.0F, 0.0F, 0.0F}});
  arena.actors.position[actor] = {4.0F, 2.0F, 0.0F};
  arena.step(600);
  REQUIRE(distanceTo(arena, actor, {0.0F, 0.0F}) <= 0.2F + 1e-4F);
  REQUIRE(arena.actors.arrived[actor] == 1);
}

TEST_CASE("a wanderer strays, but not far from home") {
  ActorArena arena;
  const uint32_t actor = arena.addActor(only(BehaviorAction::WANDER), {});
  float furthest = 0.0F;
  for (int tick = 0; tick < 1200; ++tick) {
    arena.step();
    furthest = std::max(furthest, distanceTo(arena, actor, {0.0F, 0.0F}));
  }
  REQUIRE(furthest > 1.0F);
  REQUIRE(furthest <= 4.0F + 0.3F);
}

TEST_CASE("a fleeing actor puts distance between itself and its target") {
  ActorArena arena;
  arena.addPlayer({1.0F, 0.0F});
  const uint32_t actor = arena.addActor(only(BehaviorAction::FLEE), {});
  arena.step(180);
  REQUIRE(distanceTo(arena, actor, {1.0F, 0.0F}) > 6.0F);
}

TEST_CASE("keeping distance holds the band") {
  ActorArena arena;
  arena.addPlayer({1.0F, 0.0F});
  const uint32_t actor =
      arena.addActor(only(BehaviorAction::KEEP_DISTANCE), {});
  arena.step(300);
  const float d = distanceTo(arena, actor, {1.0F, 0.0F});
  REQUIRE(d >= 3.0F);
  REQUIRE(d <= 6.0F);
}

TEST_CASE("an actor gets out of a player's way") {
  ActorArena arena;
  arena.addPlayer({0.1F, 0.0F});
  const uint32_t actor = arena.addActor(only(BehaviorAction::HOLD), {});
  arena.step(30);
  REQUIRE(distanceTo(arena, actor, {0.1F, 0.0F}) >= 0.6F - 1e-3F);
}
