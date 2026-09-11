#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>
#include <engine/math/sin-cos.h>
#include <engine/math/turn-toward.h>

using Catch::Approx;
using eng::Vec2;
using eng::math::rotateBy;
using eng::math::sinCosDegrees;
using eng::math::turnToward;

TEST_CASE("rotating by a quarter turn swaps the axes") {
  const Vec2 turned = rotateBy({1.0F, 0.0F}, sinCosDegrees(90.0F));
  REQUIRE(turned.x == 0.0F);
  REQUIRE(turned.y == 1.0F);
}

TEST_CASE("a heading within the limit lands on the desired direction") {
  const Vec2 facing =
      turnToward({1.0F, 0.0F}, {5.0F, 0.5F}, sinCosDegrees(30.0F));
  REQUIRE(facing.x == Approx(0.995037F));
  REQUIRE(facing.y == Approx(0.0995037F));
}

TEST_CASE("a heading beyond the limit turns by the limit, the short way") {
  const Vec2 left =
      turnToward({1.0F, 0.0F}, {0.0F, 1.0F}, sinCosDegrees(10.0F));
  REQUIRE(left.x == Approx(0.984808F));
  REQUIRE(left.y == Approx(0.173648F));

  const Vec2 right =
      turnToward({1.0F, 0.0F}, {0.0F, -1.0F}, sinCosDegrees(10.0F));
  REQUIRE(right.y == Approx(-0.173648F));
}

TEST_CASE("a reversal turns counterclockwise") {
  const Vec2 facing =
      turnToward({1.0F, 0.0F}, {-1.0F, 0.0F}, sinCosDegrees(45.0F));
  REQUIRE(facing.y > 0.0F);
}

TEST_CASE("a zero desired direction leaves the heading alone") {
  const Vec2 facing =
      turnToward({0.0F, 1.0F}, {0.0F, 0.0F}, sinCosDegrees(45.0F));
  REQUIRE(facing.x == 0.0F);
  REQUIRE(facing.y == 1.0F);
}

TEST_CASE("a heading turned for an hour stays unit length") {
  Vec2 facing{1.0F, 0.0F};
  const auto limit = sinCosDegrees(7.0F);
  for (int tick = 0; tick < 216000; ++tick) {
    facing = turnToward(facing, {-facing.y, facing.x}, limit);
  }
  REQUIRE(Vec2::length(facing) == Approx(1.0F).margin(1e-6));
}
