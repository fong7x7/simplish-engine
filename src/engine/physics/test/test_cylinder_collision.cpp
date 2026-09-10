#include <array>
#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>
#include <engine/physics/cylinder-collision.h>

using Catch::Approx;
using eng::Vec2;
using eng::physics::CollisionBox;
using eng::physics::CollisionCylinder;
using eng::physics::cylinderOverlapsBox;
using eng::physics::resolveCylinderAgainstBoxes;

namespace {

/// A crate: one tile across, one tall, standing on the ground at (2, 0).
constexpr CollisionBox CRATE{{2.0F, 0.0F, 0.0F}, {3.0F, 1.0F, 1.0F}};

/// A player-sized cylinder at @p x, @p y on the ground.
CollisionCylinder playerAt(float x, float y) {
  return {{x, y}, 0.3F, 0.0F, 1.5F};
}

}  // namespace

TEST_CASE("a cylinder clear of a box is left where it is") {
  const Vec2 at =
      resolveCylinderAgainstBoxes(playerAt(0.5F, 0.5F), std::span(&CRATE, 1));
  REQUIRE(at.x == 0.5F);
  REQUIRE(at.y == 0.5F);
  REQUIRE_FALSE(cylinderOverlapsBox(playerAt(0.5F, 0.5F), CRATE));
}

TEST_CASE("walking into a box's face stops against it") {
  // Centre 0.1 short of the face: 0.2 of the radius is inside the crate.
  const Vec2 at =
      resolveCylinderAgainstBoxes(playerAt(1.9F, 0.5F), std::span(&CRATE, 1));
  REQUIRE(at.x == Approx(1.7F));
  REQUIRE(at.y == Approx(0.5F));
  REQUIRE_FALSE(cylinderOverlapsBox(playerAt(at.x, at.y), CRATE));
}

TEST_CASE("walking into a box at a corner rounds it") {
  // Diagonally off the crate's (2, 0) corner, and inside the radius of it.
  const Vec2 at =
      resolveCylinderAgainstBoxes(playerAt(1.9F, -0.1F), std::span(&CRATE, 1));
  const float dx = at.x - 2.0F;
  const float dy = at.y - 0.0F;
  REQUIRE(dx * dx + dy * dy == Approx(0.09F));
  REQUIRE(at.x < 2.0F);
  REQUIRE(at.y < 0.0F);
}

TEST_CASE("a centre inside a box leaves by the nearest face") {
  const Vec2 at =
      resolveCylinderAgainstBoxes(playerAt(2.9F, 0.4F), std::span(&CRATE, 1));
  REQUIRE(at.x == Approx(3.3F));
  REQUIRE(at.y == Approx(0.4F));
}

TEST_CASE("a box above the head or flat on the floor is not in the way") {
  constexpr CollisionBox SIGN{{2.0F, 0.0F, 2.0F}, {3.0F, 1.0F, 2.5F}};
  constexpr CollisionBox DECAL{{2.0F, 0.0F, 0.0F}, {3.0F, 1.0F, 0.0F}};
  REQUIRE_FALSE(cylinderOverlapsBox(playerAt(2.5F, 0.5F), SIGN));
  REQUIRE_FALSE(cylinderOverlapsBox(playerAt(2.5F, 0.5F), DECAL));
}

TEST_CASE("a cylinder wedged between two boxes settles clear of both") {
  // Two crates with a gap of exactly one player's width between them.
  const std::array<CollisionBox, 2> pair = {
      CollisionBox{{0.0F, 0.0F, 0.0F}, {1.0F, 1.0F, 1.0F}},
      CollisionBox{{1.6F, 0.0F, 0.0F}, {2.6F, 1.0F, 1.0F}}};

  const Vec2 at = resolveCylinderAgainstBoxes(playerAt(1.2F, 0.5F), pair);

  REQUIRE_FALSE(cylinderOverlapsBox(playerAt(at.x, at.y), pair[0]));
  REQUIRE_FALSE(cylinderOverlapsBox(playerAt(at.x, at.y), pair[1]));
  REQUIRE(at.x == Approx(1.3F));
}

TEST_CASE("resolving a resolved cylinder moves it nowhere") {
  const Vec2 once =
      resolveCylinderAgainstBoxes(playerAt(1.9F, 0.5F), std::span(&CRATE, 1));
  const Vec2 twice = resolveCylinderAgainstBoxes(playerAt(once.x, once.y),
                                                 std::span(&CRATE, 1));
  REQUIRE(twice.x == once.x);
  REQUIRE(twice.y == once.y);
}
