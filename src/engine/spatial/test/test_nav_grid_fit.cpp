#include <array>
#include <catch2/catch_test_macros.hpp>
#include <engine/spatial/nav-grid-fit.h>

using eng::Vec2;
using eng::physics::CollisionBox;
using eng::spatial::fitNavGrid;
using eng::spatial::NAV_CELL_SIZE_TILES;
using eng::spatial::NAV_GRID_MARGIN_TILES;
using eng::spatial::NAV_GRID_MAX_SIDE_CELLS;

TEST_CASE("nothing to fit gives a grid with no cells") {
  const auto spec = fitNavGrid({}, {}, 0.0F);
  REQUIRE(spec.width == 0);
  REQUIRE(spec.height == 0);
}

TEST_CASE("a fitted grid covers every box and point with a margin") {
  const std::array boxes{CollisionBox{{2.0F, 1.0F, 0.0F}, {3.0F, 4.5F, 1.0F}}};
  const std::array points{Vec2{-1.5F, 6.25F}};
  const auto spec = fitNavGrid(boxes, points, 0.5F);

  REQUIRE(spec.origin.x == -1.0F - NAV_GRID_MARGIN_TILES - 1.0F);
  REQUIRE(spec.origin.y == 1.0F - NAV_GRID_MARGIN_TILES);
  REQUIRE(spec.cell_size == NAV_CELL_SIZE_TILES);
  REQUIRE(spec.floor_z == 0.5F);
  const float far_x = spec.origin.x + static_cast<float>(spec.width) * 0.25F;
  const float far_y = spec.origin.y + static_cast<float>(spec.height) * 0.25F;
  REQUIRE(far_x >= 3.0F + NAV_GRID_MARGIN_TILES);
  REQUIRE(far_y >= 6.25F + NAV_GRID_MARGIN_TILES);
}

TEST_CASE("a fitted grid's origin sits on a whole tile") {
  const std::array points{Vec2{0.3F, 0.7F}};
  const auto spec = fitNavGrid({}, points, 0.0F);
  REQUIRE(spec.origin.x == -8.0F);
  REQUIRE(spec.origin.y == -8.0F);
}

TEST_CASE("a fitted grid is never larger than the cap") {
  const std::array points{Vec2{0.0F, 0.0F}, Vec2{1000.0F, 10.0F}};
  const auto spec = fitNavGrid({}, points, 0.0F);
  REQUIRE(spec.width == NAV_GRID_MAX_SIDE_CELLS);
  REQUIRE(spec.height < NAV_GRID_MAX_SIDE_CELLS);
}
