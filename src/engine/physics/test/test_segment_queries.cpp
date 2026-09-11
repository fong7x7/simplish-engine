#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>
#include <engine/physics/segment-queries.h>

using Catch::Approx;
using eng::physics::CollisionBox;
using eng::physics::SegmentSweep;
using eng::physics::sweepHitsBox;
using eng::physics::sweepHitsCircle;

namespace {

/// A crate a tile across and a tile tall, from x = 2 to 3.
constexpr CollisionBox CRATE{{2.0F, 0.0F, 0.0F}, {3.0F, 1.0F, 1.0F}};

}  // namespace

TEST_CASE("a shot into a box stops where its edge meets the box") {
  const auto hit =
      sweepHitsBox({{0.0F, 0.5F}, {4.0F, 0.5F}, 0.1F, 0.5F}, CRATE);
  REQUIRE(hit.has_value());
  // It touches when its centre is a radius short of x = 2.
  REQUIRE(*hit == Approx(1.9F / 4.0F));
}

TEST_CASE("a shot that misses, falls short, or flies over is not stopped") {
  REQUIRE_FALSE(sweepHitsBox({{0.0F, 3.0F}, {4.0F, 3.0F}, 0.1F, 0.5F}, CRATE));
  REQUIRE_FALSE(sweepHitsBox({{0.0F, 0.5F}, {1.0F, 0.5F}, 0.1F, 0.5F}, CRATE));
  REQUIRE_FALSE(sweepHitsBox({{0.0F, 0.5F}, {4.0F, 0.5F}, 0.1F, 1.5F}, CRATE));
}

TEST_CASE("a shot that starts inside a box is stopped at once") {
  REQUIRE(sweepHitsBox({{2.5F, 0.5F}, {6.0F, 0.5F}, 0.1F, 0.5F}, CRATE) ==
          0.0F);
}

TEST_CASE("a shot meets a body where the two circles first touch") {
  const auto hit = sweepHitsCircle({{0.0F, 0.0F}, {10.0F, 0.0F}, 0.1F, 1.0F},
                                   {5.0F, 0.0F}, 0.4F);
  REQUIRE(hit.has_value());
  REQUIRE(*hit == Approx(0.45F));
}

TEST_CASE("a shot passing wide of a body, or moving away, does not hit it") {
  REQUIRE_FALSE(sweepHitsCircle({{0, 0}, {10, 0}, 0.1F, 1.0F}, {5, 2}, 0.4F));
  REQUIRE_FALSE(sweepHitsCircle({{0, 0}, {-3, 0}, 0.1F, 1.0F}, {5, 0}, 0.4F));
  REQUIRE_FALSE(sweepHitsCircle({{0, 0}, {2, 0}, 0.1F, 1.0F}, {5, 0}, 0.4F));
  REQUIRE(sweepHitsCircle({{5, 0.2F}, {9, 0}, 0.1F, 1.0F}, {5, 0}, 0.4F) ==
          0.0F);
}
