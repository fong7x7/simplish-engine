#include <catch2/catch_test_macros.hpp>
#include <engine/render-ground/ground-quarter-shape.h>

using namespace eng;
using Shape = GroundQuarterShape;
using Quarter = GroundQuarter;

namespace {

/// A grid with each of @p cells painted as terrain 1.
GroundGrid paintedAt(std::initializer_list<GroundCell> cells) {
  GroundGrid grid;
  for (const GroundCell cell : cells) {
    grid.set(cell, 1);
  }
  return grid;
}

}  // namespace

TEST_CASE("a cell painted alone is rounded at every corner") {
  const GroundGrid grid = paintedAt({{0, 0}});
  for (const GroundQuarter quarter : GROUND_QUARTERS) {
    REQUIRE(groundQuarterShape(grid, 1, {0, 0}, quarter) == Shape::ROUND);
  }
}

TEST_CASE("a straight road is full along its length and round at its ends") {
  const GroundGrid grid = paintedAt({{0, 0}, {1, 0}, {2, 0}});
  // The middle joins both ways.
  for (const GroundQuarter quarter : GROUND_QUARTERS) {
    REQUIRE(groundQuarterShape(grid, 1, {1, 0}, quarter) == Shape::FULL);
  }
  // The west end is capped round, and joins east.
  REQUIRE(groundQuarterShape(grid, 1, {0, 0}, Quarter::NORTH_WEST) ==
          Shape::ROUND);
  REQUIRE(groundQuarterShape(grid, 1, {0, 0}, Quarter::NORTH_EAST) ==
          Shape::FULL);
  // Beside it, nothing: a straight edge needs no fillet.
  REQUIRE(groundQuarterShape(grid, 1, {1, 1}, Quarter::SOUTH_EAST) ==
          Shape::EMPTY);
}

TEST_CASE("the inside of a bend is filled with a fillet") {
  // An L: west to east along y = 0, then north up x = 1.
  const GroundGrid grid = paintedAt({{0, 0}, {1, 0}, {1, 1}});
  REQUIRE(groundQuarterShape(grid, 1, {0, 1}, Quarter::SOUTH_EAST) ==
          Shape::FILLET);
  // The outside of the bend is rounded.
  REQUIRE(groundQuarterShape(grid, 1, {1, 0}, Quarter::SOUTH_EAST) ==
          Shape::ROUND);
}

TEST_CASE("cells touching only at a corner join across it") {
  const GroundGrid grid = paintedAt({{0, 0}, {1, 1}});
  REQUIRE(groundQuarterShape(grid, 1, {0, 0}, Quarter::NORTH_EAST) ==
          Shape::FULL);
  REQUIRE(groundQuarterShape(grid, 1, {1, 0}, Quarter::NORTH_WEST) ==
          Shape::FILLET);
  REQUIRE(groundQuarterShape(grid, 1, {0, 1}, Quarter::SOUTH_EAST) ==
          Shape::FILLET);
}

TEST_CASE("a later terrain counts as painted for every earlier layer") {
  GroundGrid grid;
  grid.set({0, 0}, 1);
  grid.set({1, 0}, 3);
  // Layer 1 runs on under the terrain-3 cell, so it joins there.
  REQUIRE(groundQuarterShape(grid, 1, {0, 0}, Quarter::NORTH_EAST) ==
          Shape::FULL);
  // Layer 3 does not reach back over the terrain-1 cell.
  REQUIRE(groundQuarterShape(grid, 3, {0, 0}, Quarter::NORTH_EAST) ==
          Shape::EMPTY);
  REQUIRE(groundQuarterShape(grid, 3, {1, 0}, Quarter::NORTH_WEST) ==
          Shape::ROUND);
  // Bare ground is no layer, and draws nothing.
  REQUIRE(groundQuarterShape(grid, 0, {0, 0}, Quarter::NORTH_EAST) ==
          Shape::EMPTY);
}
