#include "support/sdk-rig.h"

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>
#include <game/content/enemy-definition.h>
#include <game/sdk/actor-queries.h>
#include <game/sdk/random.h>
#include <game/sdk/ring-spawn.h>

using Catch::Approx;
using namespace eng::game;
using namespace eng::game::sdk;

TEST_CASE("a ring spawns its actors evenly round its centre") {
  uint32_t queued = 0;
  std::vector<LogicActor> ring;
  test::ScriptedLogic logic([&](GameLogicWorld& world) {
    if (world.tick() == 0) {
      queued = spawnRing(world, {.centre = {1.5F, 1.5F, 0.0F},
                                 .radius = 4.0F,
                                 .actor = {.behavior = "idle", .id = "ring"}});
    } else {
      ring = findActors(world, {.id_prefix = "ring"});
    }
  });

  test::runLogic(logic, 2, test::sdkArena(), {});

  REQUIRE(queued == 4);
  REQUIRE(ring.size() == 4);
  CHECK(ring[0].position.x == Approx(5.5F));
  CHECK(ring[1].position.y == Approx(5.5F));
}

TEST_CASE("a ring skips spots inside a prop") {
  uint32_t queued = 99;
  test::ScriptedLogic logic([&](GameLogicWorld& world) {
    if (world.tick() == 0) {
      // Every spot of a ring this small round (12.5, 1.5) is in the wall.
      queued = spawnRing(world, {.centre = {12.5F, 1.5F, 0.0F},
                                 .radius = 0.25F,
                                 .actor = {.behavior = "idle"}});
    }
  });

  test::runLogic(logic, 1, test::sdkArena(), {});

  CHECK(queued == 0);
}

namespace {

/// Content with one enemy archetype, `imp`, 6 health.
GameContent withImps() {
  GameContent content;
  EnemyDefinition imp;
  imp.id = "imp";
  imp.health = 6;
  content.enemies.push_back(imp);
  return content;
}

}  // namespace

TEST_CASE("a ring spawns the project's enemy archetype by id") {
  std::vector<LogicActor> imps;
  test::ScriptedLogic logic([&](GameLogicWorld& world) {
    if (world.tick() == 0) {
      (void)spawnRing(world, {.centre = {1.5F, 1.5F, 0.0F},
                              .enemy = "imp",
                              .actor = {.id = "imps"}});
    }
    imps = findActors(world, {.id_prefix = "imps"});
  });

  test::runLogic(logic, 2, test::sdkArena(), withImps());

  REQUIRE(imps.size() == 4);
  CHECK(imps[0].max_health == 6);
}

TEST_CASE("dice fall in range, and the same way on the same seed") {
  std::vector<int32_t> rolls[2];
  for (auto& roll : rolls) {
    test::ScriptedLogic logic([&](GameLogicWorld& world) {
      roll.push_back(between(world, -2, 2));
      roll.push_back(chance(world, 1000) && !chance(world, 0) ? 0 : 99);
    });
    test::runLogic(logic, 20, test::sdkArena(), {});
  }

  CHECK(rolls[0] == rolls[1]);
  for (const int32_t roll : rolls[0]) {
    CHECK((roll >= -2 && roll <= 2));
  }
}
