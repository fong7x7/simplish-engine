#include <array>
#include <catch2/catch_test_macros.hpp>
#include <engine/spatial/line-of-sight.h>

using eng::physics::CollisionBox;
using eng::spatial::hasLineOfSight;
using eng::spatial::NavGrid;
using eng::spatial::NavGridSpec;

namespace {

/// A grid of @p side × @p side quarter-tile cells from the world origin.
NavGridSpec square(uint32_t side) {
  return {.origin = {0.0F, 0.0F}, .width = side, .height = side};
}

/// A wall along x = 2 to 2.25 from y = 0 to 4, with a gap of @p gap tiles
/// starting at y = 2.
std::array<CollisionBox, 2> wallWithGap(float gap) {
  return {CollisionBox{{2.0F, 0.0F, 0.0F}, {2.25F, 2.0F, 1.0F}},
          CollisionBox{{2.0F, 2.0F + gap, 0.0F}, {2.25F, 4.0F, 1.0F}}};
}

}  // namespace

TEST_CASE("an open grid hides nothing") {
  const NavGrid grid(square(16), {});
  REQUIRE(hasLineOfSight(grid, {0.1F, 0.2F}, {3.9F, 3.7F}, 1));
}

TEST_CASE("a wall blocks sight and walking alike") {
  const auto wall = wallWithGap(0.0F);
  const NavGrid grid(square(16), wall);
  REQUIRE_FALSE(hasLineOfSight(grid, {1.0F, 1.0F}, {3.0F, 1.0F}, 1));
  REQUIRE_FALSE(hasLineOfSight(grid, {1.0F, 1.0F}, {3.0F, 1.0F}, 2));
  REQUIRE(hasLineOfSight(grid, {1.0F, 1.0F}, {1.5F, 3.5F}, 1));
}

TEST_CASE("a character sees through a gap it cannot walk through") {
  const auto wall = wallWithGap(0.25F);
  const NavGrid grid(square(16), wall);
  REQUIRE(hasLineOfSight(grid, {1.0F, 2.125F}, {3.0F, 2.125F}, 1));
  REQUIRE_FALSE(hasLineOfSight(grid, {1.0F, 2.125F}, {3.0F, 2.125F}, 2));
}

TEST_CASE("a line cannot slip between two cells touching at a corner") {
  // Cells (1, 1) and (2, 2) solid; (2, 1) and (1, 2) open.
  const std::array boxes{
      CollisionBox{{0.25F, 0.25F, 0.0F}, {0.5F, 0.5F, 1.0F}},
      CollisionBox{{0.5F, 0.5F, 0.0F}, {0.75F, 0.75F, 1.0F}}};
  const NavGrid grid(square(4), boxes);
  REQUIRE_FALSE(hasLineOfSight(grid, {0.625F, 0.375F}, {0.375F, 0.625F}, 1));
}

TEST_CASE("a line that starts or ends in a solid cell is blocked") {
  const auto wall = wallWithGap(0.0F);
  const NavGrid grid(square(16), wall);
  REQUIRE_FALSE(hasLineOfSight(grid, {2.1F, 1.0F}, {1.0F, 1.0F}, 1));
}

TEST_CASE("off the grid nothing is in the way") {
  const NavGrid empty;
  REQUIRE(hasLineOfSight(empty, {-50.0F, 3.0F}, {50.0F, 3.0F}, 2));
  const NavGrid grid(square(4), {});
  REQUIRE(hasLineOfSight(grid, {-2.0F, 0.5F}, {3.0F, 0.5F}, 1));
}
