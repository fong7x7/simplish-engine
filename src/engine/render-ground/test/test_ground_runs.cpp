#include <catch2/catch_test_macros.hpp>
#include <engine/render-ground/ground-runs.h>
#include <vector>

using namespace eng;

TEST_CASE("runs carry a grid across and back unchanged") {
  GroundGrid grid;
  grid.set({0, 0}, 1);
  grid.set({1, 0}, 1);
  grid.set({3, 1}, 2);
  const GroundRect bounds = grid.paintedBounds();
  const std::vector<GroundRun> runs = encodeGroundRuns(grid, bounds);
  // Row 0: 1 1 0 0; row 1: 0 0 0 2.
  REQUIRE(runs == std::vector<GroundRun>{{1, 2}, {0, 5}, {2, 1}});
  const std::optional<GroundGrid> back = decodeGroundRuns(bounds, runs);
  REQUIRE(back.has_value());
  for (int32_t y = -1; y < 3; ++y) {
    for (int32_t x = -1; x < 5; ++x) {
      REQUIRE(back->at({x, y}) == grid.at({x, y}));
    }
  }
}

TEST_CASE("runs that do not cover the rectangle exactly are refused") {
  const std::vector<GroundRun> short_runs{{1, 3}};
  const std::vector<GroundRun> long_runs{{1, 5}};
  const std::vector<GroundRun> exact{{1, 1}, {0, 3}};
  REQUIRE_FALSE(decodeGroundRuns({0, 0, 2, 2}, short_runs).has_value());
  REQUIRE_FALSE(decodeGroundRuns({0, 0, 2, 2}, long_runs).has_value());
  REQUIRE(decodeGroundRuns({0, 0, 2, 2}, exact).has_value());
}

TEST_CASE("a rectangle too large to hold is refused before it is built") {
  const std::vector<GroundRun> huge{{1, 0xFFFFFFFFu}};
  REQUIRE_FALSE(decodeGroundRuns({0, 0, 65536, 65536}, huge).has_value());
}
