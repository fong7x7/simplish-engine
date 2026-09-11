#include <catch2/catch_test_macros.hpp>
#include <game/actors/enemy-spawn.h>

using namespace eng::game;

TEST_CASE("an archetype spawns as the actor a prop given the same would be") {
  const EnemyDefinition bloater{.id = "bloater",
                                .health = 3,
                                .radius = 0.6F,
                                .height = 1.2F,
                                .behavior = "chase",
                                .faction = Faction::NEUTRAL};

  const ActorSpawn spawn = makeEnemySpawn(bloater, {2.0F, 3.0F, 0.0F}, 45.0F);

  REQUIRE(spawn.behavior == "chase");
  REQUIRE(spawn.faction == Faction::NEUTRAL);
  REQUIRE(spawn.radius == 0.6F);
  REQUIRE(spawn.height == 1.2F);
  REQUIRE(spawn.at.y == 3.0F);
  REQUIRE(spawn.yaw_degrees == 45.0F);
  REQUIRE(spawn.route.empty());
}
