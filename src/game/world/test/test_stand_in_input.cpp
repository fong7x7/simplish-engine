#include <catch2/catch_test_macros.hpp>
#include <game/player/player-system.h>
#include <game/world/stand-in-input.h>

using namespace eng;
using namespace eng::game;

namespace {

/// Two players: player 1 at @p leader, player 2 at @p stand_in; and
/// @p actors.
GameSetup twoPlayers(Vec2 leader, Vec2 stand_in,
                     std::vector<ActorSpawn> actors = {}) {
  GameSetup setup;
  setup.player_count = 2;
  setup.spawns[0] = {leader.x, leader.y, 0.0F};
  setup.spawns[1] = {stand_in.x, stand_in.y, 0.0F};
  setup.actors = std::move(actors);
  return setup;
}

}  // namespace

TEST_CASE("a stand-in far from the others comes to them") {
  const GameWorld world(twoPlayers({0, 0}, {10, 0}), {});
  const sim::PlayerInput input = standInInput(world, 1);
  REQUIRE(input.move_x < 0);
  REQUIRE(input.move_y == 0);
}

TEST_CASE("a stand-in near the others stays put") {
  const GameWorld world(twoPlayers({0, 0}, {2, 0}), {});
  const sim::PlayerInput input = standInInput(world, 1);
  REQUIRE(input.move_x == 0);
  REQUIRE(input.move_y == 0);
}

TEST_CASE("a stand-in backs away from a hostile actor, aiming at it") {
  const GameWorld world(
      twoPlayers({0, 0}, {2, 0}, {{.at = {2.0F, 2.0F, 0.0F}}}), {});
  const sim::PlayerInput input = standInInput(world, 1);
  REQUIRE(input.move_y < 0);
  REQUIRE(input.aim_y > 0);
}

TEST_CASE("a stand-in goes to revive a teammate who is down") {
  GameWorld world(twoPlayers({0, 0}, {10, 0}, {{.at = {10, 2, 0}}}), {});
  // Down player 1 by hand, as a hit would.
  auto& players = const_cast<PlayerPool&>(world.players());
  hurtPlayer(players, 0, 99, 0);
  const sim::PlayerInput input = standInInput(world, 1);
  REQUIRE(input.move_x < 0);
}

TEST_CASE("a slot with no player, or one who is down, gives no input") {
  GameWorld world(twoPlayers({0, 0}, {10, 0}), {});
  REQUIRE(standInInput(world, 3).move_x == 0);
  auto& players = const_cast<PlayerPool&>(world.players());
  hurtPlayer(players, 1, 99, 0);
  REQUIRE(standInInput(world, 1).move_x == 0);
}
