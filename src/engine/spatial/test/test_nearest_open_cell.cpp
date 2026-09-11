#include <array>
#include <catch2/catch_test_macros.hpp>
#include <engine/spatial/nearest-open-cell.h>

using eng::physics::CollisionBox;
using eng::spatial::GridCell;
using eng::spatial::NavGrid;
using eng::spatial::NavGridSpec;
using eng::spatial::nearestOpenCell;

namespace {

/// A grid of @p side × @p side quarter-tile cells from the world origin.
NavGridSpec square(uint32_t side) {
  return {.origin = {0.0F, 0.0F}, .width = side, .height = side};
}

/// A crate over cells (4..7, 4..7).
constexpr CollisionBox CRATE{{1.0F, 1.0F, 0.0F}, {2.0F, 2.0F, 1.0F}};

}  // namespace

TEST_CASE("an open cell is its own nearest") {
  const NavGrid grid(square(16), {});
  REQUIRE(nearestOpenCell(grid, {3, 3}, 1, 4) == GridCell{3, 3});
}

TEST_CASE("a cell inside a prop finds the nearest one outside it") {
  const std::array boxes{CRATE};
  const NavGrid grid(square(16), boxes);
  REQUIRE(nearestOpenCell(grid, {4, 5}, 1, 4) == GridCell{3, 5});
  // The crate's corner cell is as near the cell below as the one to its
  // left: the first in row-major order wins.
  REQUIRE(nearestOpenCell(grid, {4, 4}, 1, 4) == GridCell{4, 3});
}

TEST_CASE("a wider walker is sent further out") {
  const std::array boxes{CRATE};
  const NavGrid grid(square(16), boxes);
  REQUIRE(nearestOpenCell(grid, {4, 5}, 2, 4) == GridCell{2, 5});
}

TEST_CASE("nothing open within reach is nothing") {
  const std::array boxes{CRATE};
  const NavGrid grid(square(16), boxes);
  REQUIRE_FALSE(nearestOpenCell(grid, {5, 5}, 1, 1).has_value());
}
