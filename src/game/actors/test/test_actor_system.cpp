#include "support/actor-arena.h"

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>
#include <engine/input/input-action.h>
#include <game/actors/actor-system.h>
#include <game/content/behavior-lookup.h>

using eng::game::ActorPool;
using eng::game::compileBrain;
using eng::game::resolveBehavior;
using eng::game::spawnActor;
using eng::game::test::ActorArena;

namespace {

/// Every built-in behavior at once, around two players, among props — one
/// of whom fires on and off — so every action and most conditions run.
void populate(ActorArena& arena) {
  arena.addPlayer({3.0F, 2.0F});
  const uint32_t shooter = arena.addPlayer({-4.0F, -1.0F});
  arena.setFiring(shooter, eng::input::INPUT_BUTTON_FIRE);
  float x = -6.0F;
  for (const auto& behavior : eng::game::builtInBehaviors()) {
    // Turned toward the players, give or take, so most see one.
    arena.addActor(behavior,
                   {.at = {x, 5.0F, 0.0F}, .yaw_degrees = x * 5.0F - 90.0F});
    x += 1.5F;
  }
}

/// The crates the busy arena's actors weave between.
std::vector<eng::physics::CollisionBox> crates() {
  return {{{-2.0F, 2.5F, 0.0F}, {-1.0F, 3.5F, 1.0F}},
          {{1.0F, 3.0F, 0.0F}, {2.0F, 4.0F, 1.0F}},
          {{-5.0F, 0.0F, 0.0F}, {-4.5F, 4.0F, 1.0F}}};
}

/// Every tick's hash of the busy arena, with player 1 moved now and then.
std::vector<uint64_t> busyRunHashes() {
  ActorArena arena(crates());
  populate(arena);
  std::vector<uint64_t> hashes;
  for (int tick = 0; tick < 600; ++tick) {
    if (tick % 120 == 60) {
      arena.movePlayer(0, {static_cast<float>(tick % 7) - 3.0F, 1.0F});
    }
    arena.step();
    hashes.push_back(arena.hash());
  }
  return hashes;
}

}  // namespace

TEST_CASE(
    "a spawned actor starts at home, facing its yaw, in its first state") {
  ActorPool pool(2);
  const auto& guard = resolveBehavior({}, "guard");
  const auto handle = spawnActor(
      pool, {.at = {2.0F, 3.0F, 0.5F}, .yaw_degrees = 90.0F, .radius = 0.4F}, 0,
      compileBrain(guard));

  REQUIRE(handle.has_value());
  REQUIRE(pool.home[0].x == 2.0F);
  REQUIRE(pool.position[0].z == 0.5F);
  REQUIRE(pool.facing[0].x == 0.0F);
  REQUIRE(pool.facing[0].y == 1.0F);
  REQUIRE(pool.radius[0] == 0.4F);
  REQUIRE(pool.state[0] == guard.initial);
}

TEST_CASE("a full pool spawns nothing") {
  ActorPool pool(1);
  const auto brain = compileBrain(resolveBehavior({}, "idle"));
  REQUIRE(spawnActor(pool, {}, 0, brain).has_value());
  REQUIRE_FALSE(spawnActor(pool, {}, 0, brain).has_value());
}

TEST_CASE("compaction carries an actor's state with it") {
  ActorArena arena;
  arena.addActor(resolveBehavior({}, "idle"), {.at = {1.0F, 0.0F, 0.0F}});
  arena.addActor(resolveBehavior({}, "wander"), {.at = {5.0F, 0.0F, 0.0F}});
  const auto survivor = arena.actors.slots.handleAt(1);
  REQUIRE(arena.actors.slots.destroy(arena.actors.slots.handleAt(0)));
  eng::game::compactActors(arena.actors);

  const auto index = arena.actors.slots.denseIndex(survivor);
  REQUIRE(index == 0U);
  REQUIRE(arena.actors.home[0].x == 5.0F);
  REQUIRE(arena.actors.brain[0] == 1);
}

TEST_CASE("actors give the same hash on every tick of every run") {
  const auto first = busyRunHashes();
  REQUIRE(busyRunHashes() == first);
}

TEST_CASE("the busy arena is not trivially still") {
  ActorArena arena(crates());
  populate(arena);
  const uint64_t before = arena.hash();
  arena.step(300);
  REQUIRE(arena.hash() != before);
  uint32_t moved = 0;
  for (uint32_t i = 0; i < arena.actors.slots.size(); ++i) {
    const eng::Vec2 home = arena.actors.home[i];
    moved += eng::Vec2::distance(arena.actorAt(i), home) > 0.5F ? 1U : 0U;
  }
  // All but the idle one, which never moves.
  REQUIRE(moved >= 6);
}
