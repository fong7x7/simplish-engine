#include <algorithm>
#include <catch2/catch_test_macros.hpp>
#include <engine/core/pcg32.h>
#include <engine/spatial/neighbor-grid.h>
#include <vector>

using namespace eng;
using namespace eng::spatial;

namespace {

/// A floor 20 tiles square, its corner at (-10, -10).
NavGridSpec floorSpec() {
  return {.origin = {-10.0F, -10.0F}, .width = 80, .height = 80};
}

/// Every index @p grid visits within @p radius of @p at, in visiting order.
std::vector<uint32_t> near(const NeighborGrid& grid, Vec2 at, float radius) {
  std::vector<uint32_t> out;
  grid.forEachNear(at, radius, [&](uint32_t i) { out.push_back(i); });
  return out;
}

/// @p count points scattered over and a little past the floor.
std::vector<Vec2> scattered(uint32_t count) {
  Pcg32 rng(3, 3);
  std::vector<Vec2> points;
  for (uint32_t i = 0; i < count; ++i) {
    points.push_back({rng.nextUnitFloat() * 24.0F - 12.0F,
                      rng.nextUnitFloat() * 24.0F - 12.0F});
  }
  return points;
}

}  // namespace

TEST_CASE("every point within the radius is visited, and once") {
  const std::vector<Vec2> points = scattered(500);
  NeighborGrid grid(floorSpec(), 500);
  grid.rebuild(points);

  for (const Vec2 at : {Vec2{0, 0}, Vec2{-9.5F, 9.5F}, Vec2{11, -11}}) {
    std::vector<uint32_t> visited = near(grid, at, 1.5F);
    std::vector<uint32_t> sorted = visited;
    std::ranges::sort(sorted);
    REQUIRE(std::ranges::adjacent_find(sorted) == sorted.end());
    for (uint32_t i = 0; i < points.size(); ++i) {
      if (Vec2::distanceSquared(points[i], at) <= 1.5F * 1.5F) {
        REQUIRE(std::ranges::binary_search(sorted, i));
      }
    }
  }
}

TEST_CASE("a query visits far fewer points than there are") {
  NeighborGrid grid(floorSpec(), 500);
  grid.rebuild(scattered(500));
  REQUIRE(near(grid, {0, 0}, 0.6F).size() < 50);
}

TEST_CASE("points in one bucket are visited in index order") {
  NeighborGrid grid(floorSpec(), 4);
  grid.rebuild(
      std::vector<Vec2>{{0.5F, 0.5F}, {5, 5}, {0.2F, 0.7F}, {0.9F, 0.1F}});
  REQUIRE(near(grid, {0.5F, 0.5F}, 0.1F) == std::vector<uint32_t>{0, 2, 3});
}

TEST_CASE("a rebuild replaces what the grid held") {
  NeighborGrid grid(floorSpec(), 2);
  grid.rebuild(std::vector<Vec2>{{0, 0}, {0, 0}});
  grid.rebuild(std::vector<Vec2>{{5, 5}});
  REQUIRE(near(grid, {0, 0}, 0.5F).empty());
  REQUIRE(near(grid, {5, 5}, 0.5F) == std::vector<uint32_t>{0});
}

TEST_CASE("a point off the floor is found at the edge it is beyond") {
  NeighborGrid grid(floorSpec(), 1);
  grid.rebuild(std::vector<Vec2>{{40.0F, 0.0F}});
  REQUIRE(near(grid, {9.8F, 0.0F}, 0.5F) == std::vector<uint32_t>{0});
}

TEST_CASE("a grid of one bucket visits everything") {
  NeighborGrid grid;
  grid.rebuild(std::vector<Vec2>{{-100, 3}, {100, 7}});
  REQUIRE(near(grid, {0, 0}, 0.1F) == std::vector<uint32_t>{0, 1});
}
