#include "support/sdk-rig.h"

#include <catch2/catch_test_macros.hpp>
#include <game/sdk/actor-queries.h>
#include <game/sdk/player-queries.h>
#include <string>
#include <vector>

using namespace eng::game;
using namespace eng::game::sdk;

namespace {

/// Ask @p ask of the arena on tick 0.
void askArena(const std::function<void(GameLogicWorld&)>& ask) {
  test::ScriptedLogic logic(ask);
  test::runLogic(logic, 1, test::sdkArena(), {});
}

/// The ids of @p actors, in order.
std::vector<std::string> idsOf(const std::vector<LogicActor>& actors) {
  std::vector<std::string> ids;
  for (const LogicActor& actor : actors) {
    ids.emplace_back(actor.id);
  }
  return ids;
}

}  // namespace

TEST_CASE("actors are found by name, by side, and by the start of a name") {
  askArena([](GameLogicWorld& world) {
    CHECK(findActor(world, "villager")->faction == Faction::NEUTRAL);
    CHECK_FALSE(findActor(world, "nobody").has_value());
    CHECK(countActors(world, {.faction = Faction::HOSTILE}) == 2);
    CHECK(idsOf(findActors(world, {.id_prefix = "grunt"})) ==
          std::vector<std::string>{"grunt_a", "grunt_b"});
  });
}

TEST_CASE("actors are found by where they stand") {
  askArena([](GameLogicWorld& world) {
    const eng::Vec3 player = world.player(0).position;
    CHECK(idsOf(actorsWithin(world, player, 5.0F, {})) ==
          std::vector<std::string>{"grunt_a", "villager"});
    CHECK(nearestActor(world, {9.0F, 1.5F, 0.0F}, {})->id == "grunt_b");
    CHECK(nearestActor(world, player, {.faction = Faction::NEUTRAL})->id ==
          "villager");
  });
}

TEST_CASE("players are found by whether they are up, and where") {
  askArena([](GameLogicWorld& world) {
    CHECK(playersUp(world).size() == 1);
    CHECK(nearestPlayer(world, {20.0F, 0.0F, 0.0F})->slot == 0);
    CHECK(playersCentre(world).x == 1.5F);
  });
}
