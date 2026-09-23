#include <catch2/catch_test_macros.hpp>
#include <engine/render-ground/ground-region.h>

using namespace eng;

TEST_CASE("a region is every cell of its terrain joined to the seed") {
  GroundGrid grid;
  // An L of sand, a separate grass patch touching it, and a far sand cell.
  grid.set({0, 0}, 3);
  grid.set({1, 0}, 3);
  grid.set({1, 1}, 3);
  grid.set({2, 1}, 1);
  grid.set({9, 9}, 3);

  const std::vector<GroundCell> sand = connectedGroundCells(grid, {0, 0});
  REQUIRE(sand == std::vector<GroundCell>{{0, 0}, {1, 0}, {1, 1}});
  REQUIRE(connectedGroundCells(grid, {2, 1}) ==
          std::vector<GroundCell>{{2, 1}});
}

TEST_CASE("cells touching at a corner are one region, as they are drawn") {
  GroundGrid grid;
  for (int32_t i = 0; i < 4; ++i) {
    grid.set({i, i}, 6);
  }
  REQUIRE(connectedGroundCells(grid, {3, 3}).size() == 4);
}

TEST_CASE("bare ground is no region") {
  GroundGrid grid;
  grid.set({0, 0}, 2);
  REQUIRE(connectedGroundCells(grid, {5, 5}).empty());
  REQUIRE(connectedGroundCells(GroundGrid{}, {0, 0}).empty());
}
