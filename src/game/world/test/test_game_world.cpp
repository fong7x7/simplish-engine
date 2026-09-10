#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>
#include <engine/input/player-input-builder.h>
#include <engine/sim/simulation.h>
#include <game/player/player-system.h>
#include <game/world/game-world.h>

using Catch::Approx;
using eng::game::GameSetup;
using eng::game::GameWorld;
using eng::sim::Simulation;
using eng::sim::TickHashing;
using eng::sim::TickInput;

namespace {

GameSetup twoPlayers() {
  GameSetup setup;
  setup.player_count = 2;
  setup.spawns[0] = {1.5F, 2.5F, 0.0F};
  setup.spawns[1] = {6.5F, 2.5F, 0.0F};
  return setup;
}

}  // namespace

TEST_CASE("a world starts with one player per slot, at its spawn") {
  const GameWorld world(twoPlayers());

  REQUIRE(world.players().slots.size() == 2);
  REQUIRE(world.players().position[1].x == 6.5F);
  REQUIRE(world.players().input_slot[1] == 1);
}

TEST_CASE("a world holds at least one player and at most four") {
  GameSetup none;
  none.player_count = 0;
  GameSetup many;
  many.player_count = 9;

  REQUIRE(GameWorld(none).players().slots.size() == 1);
  REQUIRE(GameWorld(many).players().slots.size() == 4);
}

TEST_CASE("stepping the world moves the player whose stick is pushed") {
  GameWorld world(twoPlayers());
  Simulation simulation(world, TickHashing::ON);
  TickInput input;
  input.players[1].move_x = eng::input::INPUT_AXIS_MAX;

  for (int tick = 0; tick < 60; ++tick) {
    (void)simulation.step(input);
  }

  // Sixty ticks at full stick is one second: five tiles.
  REQUIRE(world.players().position[1].x ==
          Approx(6.5F + 60.0F * eng::game::PLAYER_SPEED_TILES_PER_TICK));
  REQUIRE(world.players().position[0].x == 1.5F);
}

TEST_CASE("a world's tick hash names its players section") {
  GameWorld world(twoPlayers());
  Simulation simulation(world, TickHashing::ON);

  const auto hash = simulation.step(TickInput{}).hash;

  REQUIRE(hash.has_value());
  REQUIRE(hash->section_count == 1);
  REQUIRE(hash->sections[0].name == "players");
}

TEST_CASE("a world keeps its players out of the setup's obstacles") {
  GameSetup setup = twoPlayers();
  // A crate one tile right of player 1's spawn.
  setup.obstacles.push_back({{2.5F, 2.0F, 0.0F}, {3.5F, 3.0F, 1.0F}});
  GameWorld world(setup);
  Simulation simulation(world, TickHashing::ON);
  TickInput input;
  input.players[0].move_x = eng::input::INPUT_AXIS_MAX;

  for (int tick = 0; tick < 60; ++tick) {
    (void)simulation.step(input);
  }

  REQUIRE(world.players().position[0].x ==
          Approx(2.5F - eng::game::PLAYER_RADIUS_TILES));
}
