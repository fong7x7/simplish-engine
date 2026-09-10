#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>
#include <engine/input/player-input-builder.h>
#include <game/player/player-system.h>
#include <span>

using Catch::Approx;
using eng::Vec3;
using eng::game::PLAYER_POOL_CAPACITY;
using eng::game::PLAYER_SPEED_TILES_PER_TICK;
using eng::game::PlayerPool;
using eng::input::INPUT_AXIS_MAX;
using eng::sim::TickInput;

namespace {

/// A tick where input slot @p slot pushes the stick to (@p x, @p y).
TickInput stick(uint8_t slot, int16_t x, int16_t y) {
  TickInput input;
  input.players[slot].move_x = x;
  input.players[slot].move_y = y;
  return input;
}

}  // namespace

TEST_CASE("a spawned player stands where it was put, aiming along +X") {
  PlayerPool pool;
  REQUIRE(eng::game::spawnPlayer(pool, 0, {3.5F, 4.5F, 1.0F}).has_value());

  REQUIRE(pool.slots.size() == 1);
  REQUIRE(pool.position[0].x == 3.5F);
  REQUIRE(pool.position[0].z == 1.0F);
  REQUIRE(pool.aim[0].x == 1.0F);
  REQUIRE(pool.input_slot[0] == 0);
}

TEST_CASE("the pool holds one player per input slot and no more") {
  PlayerPool pool;
  for (uint32_t i = 0; i < PLAYER_POOL_CAPACITY; ++i) {
    REQUIRE(eng::game::spawnPlayer(pool, static_cast<uint8_t>(i), {}));
  }
  REQUIRE_FALSE(eng::game::spawnPlayer(pool, 0, {}).has_value());
}

TEST_CASE("a full stick moves a player one tick's worth of speed") {
  PlayerPool pool;
  (void)eng::game::spawnPlayer(pool, 0, {});

  eng::game::movePlayers(pool, stick(0, INPUT_AXIS_MAX, -INPUT_AXIS_MAX), {});

  REQUIRE(pool.position[0].x == Approx(PLAYER_SPEED_TILES_PER_TICK));
  REQUIRE(pool.position[0].y == Approx(-PLAYER_SPEED_TILES_PER_TICK));
  REQUIRE(pool.position[0].z == 0.0F);
}

TEST_CASE("each player follows its own input slot") {
  PlayerPool pool;
  (void)eng::game::spawnPlayer(pool, 0, {});
  (void)eng::game::spawnPlayer(pool, 2, {});

  eng::game::movePlayers(pool, stick(2, INPUT_AXIS_MAX, 0), {});

  REQUIRE(pool.position[0].x == 0.0F);
  REQUIRE(pool.position[1].x == Approx(PLAYER_SPEED_TILES_PER_TICK));
}

TEST_CASE("a player keeps its aim while no aim is given") {
  PlayerPool pool;
  (void)eng::game::spawnPlayer(pool, 0, {});
  TickInput aiming;
  aiming.players[0].aim_y = INPUT_AXIS_MAX;

  eng::game::movePlayers(pool, aiming, {});
  REQUIRE(pool.aim[0].y == Approx(1.0F));
  eng::game::movePlayers(pool, TickInput{}, {});
  REQUIRE(pool.aim[0].y == Approx(1.0F));
  REQUIRE(pool.aim[0].x == Approx(0.0F));
}

TEST_CASE("a destroyed player is gone after compaction, the rest kept") {
  PlayerPool pool;
  const auto first = eng::game::spawnPlayer(pool, 0, {1.0F, 0.0F, 0.0F});
  (void)eng::game::spawnPlayer(pool, 1, {2.0F, 0.0F, 0.0F});

  REQUIRE(pool.slots.destroy(*first));
  eng::game::compactPlayers(pool);

  REQUIRE(pool.slots.size() == 1);
  REQUIRE(pool.position[0].x == 2.0F);
  REQUIRE(pool.input_slot[0] == 1);
}

TEST_CASE("players hash differently once one has moved") {
  PlayerPool a;
  PlayerPool b;
  (void)eng::game::spawnPlayer(a, 0, {});
  (void)eng::game::spawnPlayer(b, 0, {});
  const auto hashOf = [](const PlayerPool& pool) {
    eng::sim::StateHasher hasher;
    eng::game::hashPlayers(pool, hasher);
    return hasher.value();
  };
  REQUIRE(hashOf(a) == hashOf(b));
  eng::game::movePlayers(a, stick(0, 1, 0), {});
  REQUIRE(hashOf(a) != hashOf(b));
}

namespace {

/// A wall two tiles right of the origin, one tile thick and tall.
constexpr eng::physics::CollisionBox WALL{{2.0F, -5.0F, 0.0F},
                                          {3.0F, 5.0F, 1.0F}};

}  // namespace

TEST_CASE("a player walking into a wall stops against it") {
  PlayerPool pool;
  (void)eng::game::spawnPlayer(pool, 0, {1.5F, 0.0F, 0.0F});

  for (int tick = 0; tick < 60; ++tick) {
    eng::game::movePlayers(pool, stick(0, INPUT_AXIS_MAX, 0),
                           std::span(&WALL, 1));
  }

  REQUIRE(pool.position[0].x == Approx(2.0F - eng::game::PLAYER_RADIUS_TILES));
}

TEST_CASE("a player walking into a wall at an angle slides along it") {
  PlayerPool pool;
  (void)eng::game::spawnPlayer(pool, 0, {1.5F, 0.0F, 0.0F});
  const int16_t diagonal = 23170;

  for (int tick = 0; tick < 12; ++tick) {
    eng::game::movePlayers(pool, stick(0, diagonal, diagonal),
                           std::span(&WALL, 1));
  }

  // Held at the wall in X, and still covering ground in Y.
  REQUIRE(pool.position[0].x == Approx(2.0F - eng::game::PLAYER_RADIUS_TILES));
  REQUIRE(pool.position[0].y > 0.5F);
}

TEST_CASE("a player spawned inside an obstacle is out of it after a tick") {
  PlayerPool pool;
  (void)eng::game::spawnPlayer(pool, 0, {2.9F, 0.0F, 0.0F});

  eng::game::movePlayers(pool, TickInput{}, std::span(&WALL, 1));

  REQUIRE(pool.position[0].x == Approx(3.0F + eng::game::PLAYER_RADIUS_TILES));
}
