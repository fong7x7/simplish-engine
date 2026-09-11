#include <array>
#include <catch2/catch_test_macros.hpp>
#include <engine/spatial/reachability.h>

using eng::physics::CollisionBox;
using eng::spatial::GridCell;
using eng::spatial::NavGrid;
using eng::spatial::NavGridSpec;
using eng::spatial::reachableCells;

namespace {

/// A grid of @p side × @p side quarter-tile cells from the world origin.
NavGridSpec square(uint32_t side) {
  return {.origin = {0.0F, 0.0F}, .width = side, .height = side};
}

/// A sealed box ring round cells (10..13, 10..13) of a 16-cell grid.
std::array<CollisionBox, 4> ring() {
  return {CollisionBox{{2.25F, 2.25F, 0.0F}, {3.75F, 2.5F, 1.0F}},
          CollisionBox{{2.25F, 3.5F, 0.0F}, {3.75F, 3.75F, 1.0F}},
          CollisionBox{{2.25F, 2.25F, 0.0F}, {2.5F, 3.75F, 1.0F}},
          CollisionBox{{3.5F, 2.25F, 0.0F}, {3.75F, 3.75F, 1.0F}}};
}

}  // namespace

TEST_CASE("everything open is reachable across an empty grid") {
  const NavGrid grid(square(8), {});
  const std::array sources{GridCell{0, 0}};
  for (const uint8_t reached : reachableCells(grid, sources, 1)) {
    REQUIRE(reached == 1);
  }
}

TEST_CASE("a sealed room is unreachable from outside it, and its walls too") {
  const auto walls = ring();
  const NavGrid grid(square(16), walls);
  const std::array sources{GridCell{0, 0}};
  const auto reached = reachableCells(grid, sources, 1);
  REQUIRE(reached[grid.indexOf({1, 1})] == 1);
  REQUIRE(reached[grid.indexOf({12, 12})] == 0);
  REQUIRE(reached[grid.indexOf({9, 12})] == 0);

  const std::array inside{GridCell{12, 12}};
  const auto from_inside = reachableCells(grid, inside, 1);
  REQUIRE(from_inside[grid.indexOf({11, 11})] == 1);
  REQUIRE(from_inside[grid.indexOf({1, 1})] == 0);
}

TEST_CASE("a gap too narrow for the clearance cuts the floor in two") {
  // A wall across column 8 with a two-cell gap at rows 7 and 8.
  const std::array walls{
      CollisionBox{{2.0F, 0.0F, 0.0F}, {2.25F, 1.75F, 1.0F}},
      CollisionBox{{2.0F, 2.25F, 0.0F}, {2.25F, 4.0F, 1.0F}}};
  const NavGrid grid(square(16), walls);
  const std::array sources{GridCell{2, 8}};
  REQUIRE(reachableCells(grid, sources, 1)[grid.indexOf({13, 8})] == 1);
  REQUIRE(reachableCells(grid, sources, 2)[grid.indexOf({13, 8})] == 0);
}

TEST_CASE("a source that is not open reaches nothing") {
  const auto walls = ring();
  const NavGrid grid(square(16), walls);
  const std::array sources{GridCell{9, 9}};
  for (const uint8_t reached : reachableCells(grid, sources, 1)) {
    REQUIRE(reached == 0);
  }
}
