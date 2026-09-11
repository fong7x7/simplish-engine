#include <catch2/catch_test_macros.hpp>
#include <game/content/enemy-lookup.h>

using namespace eng::game;

TEST_CASE("an archetype is found by its id, and nothing by another") {
  GameContent content;
  content.enemies.push_back({.id = "swarmer", .behavior = "chase"});
  content.enemies.push_back({.id = "charger", .behavior = "charger"});

  REQUIRE(findEnemy(content, "charger") == &content.enemies[1]);
  REQUIRE(findEnemy(content, "bloater") == nullptr);
  REQUIRE(findEnemy(content, "") == nullptr);
}
