#include <array>
#include <catch2/catch_test_macros.hpp>
#include <engine/spatial/line-of-sight.h>
#include <engine/spatial/path-finder.h>
#include <engine/spatial/path-smoothing.h>

using eng::Vec2;
using eng::physics::CollisionBox;
using eng::spatial::GridCell;
using eng::spatial::hasLineOfSight;
using eng::spatial::NavGrid;
using eng::spatial::NavGridSpec;
using eng::spatial::PathFinder;
using eng::spatial::smoothPath;

namespace {

/// A grid of @p side × @p side quarter-tile cells from the world origin.
NavGridSpec square(uint32_t side) {
  return {.origin = {0.0F, 0.0F}, .width = side, .height = side};
}

}  // namespace

TEST_CASE("a path across open floor smooths to one straight leg") {
  const NavGrid grid(square(16), {});
  PathFinder finder;
  const auto path = finder.find(grid, {.from = {0, 0}, .to = {12, 5}});
  std::array<Vec2, 16> waypoints{};

  REQUIRE(smoothPath(grid, path.cells, 1, waypoints) == 1);
  REQUIRE(waypoints[0].x == grid.centre({12, 5}).x);
  REQUIRE(waypoints[0].y == grid.centre({12, 5}).y);
}

TEST_CASE("a path round a wall smooths to legs each walkable in a line") {
  const std::array wall{CollisionBox{{2.0F, 0.0F, 0.0F}, {2.25F, 3.0F, 1.0F}}};
  const NavGrid grid(square(16), wall);
  PathFinder finder;
  const auto path = finder.find(grid, {.from = {2, 2}, .to = {13, 2}});
  std::array<Vec2, 16> waypoints{};
  const size_t count = smoothPath(grid, path.cells, 1, waypoints);

  REQUIRE(count >= 2);
  REQUIRE(count < path.cells.size() - 1);
  Vec2 from = grid.centre({2, 2});
  for (size_t i = 0; i < count; ++i) {
    REQUIRE(hasLineOfSight(grid, from, waypoints[i], 1));
    from = waypoints[i];
  }
  REQUIRE(waypoints[count - 1].x == grid.centre({13, 2}).x);
}

TEST_CASE("smoothing stops when the waypoint buffer is full") {
  const std::array wall{CollisionBox{{2.0F, 0.0F, 0.0F}, {2.25F, 3.0F, 1.0F}}};
  const NavGrid grid(square(16), wall);
  PathFinder finder;
  const auto path = finder.find(grid, {.from = {2, 2}, .to = {13, 2}});
  std::array<Vec2, 1> one{};
  REQUIRE(smoothPath(grid, path.cells, 1, one) == 1);
  REQUIRE_FALSE(one[0].x == grid.centre({13, 2}).x);
}

TEST_CASE("a path of one cell has no waypoints") {
  const NavGrid grid(square(4), {});
  const std::array cells{GridCell{1, 1}};
  std::array<Vec2, 4> waypoints{};
  REQUIRE(smoothPath(grid, cells, 1, waypoints) == 0);
}
