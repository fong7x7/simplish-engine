// Players being hurt, going down, being revived, and going out.

#include <catch2/catch_test_macros.hpp>
#include <game/content/character-lookup.h>
#include <game/player/player-system.h>

using namespace eng::game;

namespace {

/// A pool of @p count default-character players in a row a tile apart.
PlayerPool playersInARow(uint8_t count) {
  PlayerPool pool;
  for (uint8_t slot = 0; slot < count; ++slot) {
    (void)spawnPlayer(pool, slot, {static_cast<float>(slot), 0.0F, 0.0F},
                      defaultCharacter());
  }
  return pool;
}

}  // namespace

TEST_CASE("a hit takes segments, then there is a moment's grace") {
  PlayerPool pool = playersInARow(1);
  hurtPlayer(pool, 0, 2, 100);
  REQUIRE(pool.health[0] == DEFAULT_CHARACTER_HEALTH - 2);
  hurtPlayer(pool, 0, 1, 100 + PLAYER_HURT_GRACE_TICKS - 1);
  REQUIRE(pool.health[0] == DEFAULT_CHARACTER_HEALTH - 2);
  hurtPlayer(pool, 0, 1, 100 + PLAYER_HURT_GRACE_TICKS);
  REQUIRE(pool.health[0] == DEFAULT_CHARACTER_HEALTH - 3);
}

TEST_CASE("a player out of health goes down, and a down player takes no more") {
  PlayerPool pool = playersInARow(2);
  hurtPlayer(pool, 0, 99, 10);
  REQUIRE(pool.health[0] == 0);
  REQUIRE(pool.downed[0] == 1);
  REQUIRE_FALSE(playerIsUp(pool, 0));
  hurtPlayer(pool, 0, 1, 500);
  REQUIRE(pool.downed[0] == 1);
}

TEST_CASE("a teammate standing by long enough revives a downed player") {
  PlayerPool pool = playersInARow(2);
  hurtPlayer(pool, 0, 99, 0);
  for (uint64_t tick = 1; tick < PLAYER_REVIVE_TICKS; ++tick) {
    updateDownedPlayers(pool, tick);
  }
  REQUIRE(pool.downed[0] == 1);
  updateDownedPlayers(pool, PLAYER_REVIVE_TICKS);
  REQUIRE(playerIsUp(pool, 0));
  REQUIRE(pool.health[0] == PLAYER_REVIVED_HEALTH);
}

TEST_CASE("a downed player nobody reaches is out when the window closes") {
  PlayerPool pool = playersInARow(2);
  pool.position[1] = {10.0F, 0.0F, 0.0F};
  hurtPlayer(pool, 0, 99, 0);
  updateDownedPlayers(pool, PLAYER_DOWNED_WINDOW_TICKS - 1);
  REQUIRE(pool.downed[0] == 1);
  updateDownedPlayers(pool, PLAYER_DOWNED_WINDOW_TICKS);
  REQUIRE(pool.out[0] == 1);
  REQUIRE(pool.downed[0] == 0);
}

TEST_CASE("a player down alone is out at once: solo death ends the run") {
  PlayerPool pool = playersInARow(1);
  hurtPlayer(pool, 0, 99, 0);
  updateDownedPlayers(pool, 1);
  REQUIRE(pool.out[0] == 1);
}

TEST_CASE("a player who is not up does not move") {
  PlayerPool pool = playersInARow(1);
  hurtPlayer(pool, 0, 99, 0);
  eng::sim::TickInput input;
  input.players[0].move_x = 1000;
  movePlayers(pool, input, {});
  REQUIRE(pool.position[0].x == 0.0F);
}
