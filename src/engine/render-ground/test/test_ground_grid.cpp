#include <catch2/catch_test_macros.hpp>
#include <engine/render-ground/ground-grid.h>
#include <vector>

using namespace eng;

TEST_CASE("an empty grid is bare everywhere") {
  const GroundGrid grid;
  REQUIRE(grid.empty());
  REQUIRE(grid.at({0, 0}) == 0);
  REQUIRE(grid.at({-40, 900}) == 0);
  REQUIRE(grid.paintedBounds() == GroundRect{});
}

TEST_CASE("painting a cell grows the grid to hold it") {
  GroundGrid grid;
  REQUIRE(grid.set({3, -5}, 2));
  REQUIRE(grid.at({3, -5}) == 2);
  REQUIRE(grid.at({4, -5}) == 0);
  REQUIRE_FALSE(grid.empty());
  // A block at a time, so a stroke outward does not reallocate per cell.
  REQUIRE(grid.bounds().width % GROUND_GROWTH_BLOCK == 0);
  REQUIRE(grid.bounds().height % GROUND_GROWTH_BLOCK == 0);
  REQUIRE(grid.paintedBounds() == GroundRect{3, -5, 1, 1});
}

TEST_CASE("growing keeps every cell already painted where it was") {
  GroundGrid grid;
  grid.set({0, 0}, 1);
  grid.set({1, 2}, 3);
  grid.set({-30, 40}, 2);
  grid.set({50, -20}, 4);
  REQUIRE(grid.at({0, 0}) == 1);
  REQUIRE(grid.at({1, 2}) == 3);
  REQUIRE(grid.at({-30, 40}) == 2);
  REQUIRE(grid.at({50, -20}) == 4);
  REQUIRE(grid.paintedBounds() == GroundRect{-30, -20, 81, 61});
}

TEST_CASE("setting what a cell already holds changes nothing") {
  GroundGrid grid;
  REQUIRE_FALSE(grid.set({7, 7}, 0));
  REQUIRE(grid.empty());
  REQUIRE(grid.bounds().width == 0);
  grid.set({7, 7}, 1);
  REQUIRE_FALSE(grid.set({7, 7}, 1));
}

TEST_CASE("a cell past the coordinate limit is refused") {
  GroundGrid grid;
  REQUIRE_FALSE(grid.set({GROUND_COORDINATE_LIMIT, 0}, 1));
  REQUIRE_FALSE(grid.set({0, -GROUND_COORDINATE_LIMIT - 1}, 1));
  REQUIRE(grid.empty());
  REQUIRE(grid.set({GROUND_COORDINATE_LIMIT - 1, -GROUND_COORDINATE_LIMIT}, 1));
}

TEST_CASE("a grid is rebuilt from its cells only when the count fits") {
  REQUIRE(GroundGrid::fromCells({0, 0, 2, 2}, {1, 0, 0, 1}).has_value());
  REQUIRE_FALSE(GroundGrid::fromCells({0, 0, 2, 2}, {1, 0, 0}).has_value());
  REQUIRE_FALSE(GroundGrid::fromCells({GROUND_COORDINATE_LIMIT, 0, 1, 1}, {1})
                    .has_value());
  const GroundGrid grid = *GroundGrid::fromCells({5, 6, 2, 1}, {0, 3});
  // Row by row from the south-west.
  REQUIRE(grid.at({5, 6}) == 0);
  REQUIRE(grid.at({6, 6}) == 3);
}
