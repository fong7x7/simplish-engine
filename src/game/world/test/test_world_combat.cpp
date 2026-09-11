// Consequences, end to end: the presets' attacks landing on players and
// actors through the effects buffer, the damage phase and the pools.

#include <catch2/catch_test_macros.hpp>
#include <engine/sim/simulation.h>
#include <game/content/character-lookup.h>
#include <game/player/player-system.h>
#include <game/world/game-world.h>

using namespace eng;
using namespace eng::game;

namespace {

/// One player standing at the origin, and @p actors.
GameSetup playerAnd(std::vector<ActorSpawn> actors) {
  GameSetup setup;
  setup.spawns[0] = {0.0F, 0.0F, 0.0F};
  setup.actors = std::move(actors);
  return setup;
}

/// A world of @p setup stepped @p ticks idle ticks.
void run(GameWorld& world, uint64_t ticks) {
  sim::Simulation simulation(world, sim::TickHashing::OFF);
  for (uint64_t t = 0; t < ticks; ++t) {
    (void)simulation.step({});
  }
}

/// An actor running @p behavior at @p x, 0, facing +X — toward the player
/// from the negative side.
ActorSpawn at(float x, const char* behavior) {
  return {.at = {x, 0.0F, 0.0F}, .behavior = behavior};
}

}  // namespace

TEST_CASE("a swarmer bites a lone player down, a segment a grace, and the "
          "run is over") {
  GameWorld world(playerAnd({at(3.0F, "chase")}), {});
  run(world, 60);
  REQUIRE(world.players().health[0] < DEFAULT_CHARACTER_HEALTH);
  REQUIRE_FALSE(world.runOver());
  run(world, PLAYER_HURT_GRACE_TICKS * DEFAULT_CHARACTER_HEALTH + 60);
  REQUIRE(world.players().health[0] == 0);
  REQUIRE(world.players().out[0] == 1);
  REQUIRE(world.runOver());
}

TEST_CASE("a bloater bursts beside the player, hurting its own side too") {
  GameWorld world(playerAnd({at(4.0F, "bloater"), at(1.2F, "idle")}), {});
  run(world, 240);
  REQUIRE(world.players().health[0] == DEFAULT_CHARACTER_HEALTH - 2);
  // The bloater is gone, and the idler beside the player took the blast.
  REQUIRE(world.actors().slots.size() == 1);
  REQUIRE(world.actors().health[0] == ACTOR_DEFAULT_HEALTH - 2);
}

TEST_CASE("a skirmisher's volleys fly and land on the player") {
  GameWorld world(playerAnd({at(-5.0F, "skirmisher")}), {});
  bool saw_a_shot = false;
  sim::Simulation simulation(world, sim::TickHashing::OFF);
  for (int t = 0; t < 400; ++t) {
    (void)simulation.step({});
    saw_a_shot = saw_a_shot || world.projectilePool().slots.size() > 0;
  }
  REQUIRE(saw_a_shot);
  REQUIRE(world.players().health[0] < DEFAULT_CHARACTER_HEALTH);
}

TEST_CASE("a spitter's pool lies where the player stood, and bites them") {
  GameWorld world(playerAnd({at(-7.0F, "spitter")}), {});
  bool saw_a_pool = false;
  sim::Simulation simulation(world, sim::TickHashing::OFF);
  for (int t = 0; t < 300; ++t) {
    (void)simulation.step({});
    saw_a_pool = saw_a_pool || world.hazardPool().slots.size() > 0;
  }
  REQUIRE(saw_a_pool);
  REQUIRE(world.players().health[0] < DEFAULT_CHARACTER_HEALTH);
}

TEST_CASE("a defender takes on a hostile actor and kills it") {
  GameSetup setup = playerAnd({{.at = {-6.0F, 0.0F, 0.0F},
                                .behavior = "defender",
                                .faction = Faction::FRIENDLY},
                               {.at = {-2.0F, 3.0F, 0.0F},
                                .behavior = "idle",
                                .faction = Faction::HOSTILE}});
  GameWorld world(setup, {});
  run(world, 600);
  REQUIRE(world.actors().slots.size() == 1);
  REQUIRE(world.actors().faction[0] == Faction::FRIENDLY);
}
