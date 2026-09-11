#include <array>
#include <catch2/catch_test_macros.hpp>
#include <engine/spatial/nav-grid.h>

using eng::physics::CollisionBox;
using eng::spatial::GridCell;
using eng::spatial::NavGrid;
using eng::spatial::NavGridSpec;

namespace {

/// A grid of @p side × @p side quarter-tile cells from the world origin.
NavGridSpec square(uint32_t side) {
  return {.origin = {0.0F, 0.0F}, .width = side, .height = side};
}

/// A crate one tile across and one tall, its near corner at @p x, @p y.
CollisionBox crateAt(float x, float y) {
  return {{x, y, 0.0F}, {x + 1.0F, y + 1.0F, 1.0F}};
}

}  // namespace

TEST_CASE("an empty grid holds no cells") {
  const NavGrid grid;
  REQUIRE(grid.cellCount() == 0);
  REQUIRE_FALSE(grid.cellAt({0.0F, 0.0F}).has_value());
  REQUIRE(grid.clearance({0, 0}) == 0);
}

TEST_CASE("a box makes exactly the cells it covers solid") {
  const std::array boxes{crateAt(1.0F, 1.0F)};
  const NavGrid grid(square(16), boxes);

  REQUIRE(grid.clearance({4, 4}) == 0);
  REQUIRE(grid.clearance({7, 7}) == 0);
  // Its edges lie on cell edges, so the cells beside it are open.
  REQUIRE(grid.clearance({3, 4}) == 1);
  REQUIRE(grid.clearance({8, 7}) == 1);
  REQUIRE(grid.clearance({2, 4}) == 2);
  REQUIRE(grid.clearance({0, 0}) == 4);
}

TEST_CASE("a box edge inside a cell makes the whole cell solid") {
  const std::array boxes{CollisionBox{{1.1F, 1.1F, 0.0F}, {1.9F, 1.9F, 1.0F}}};
  const NavGrid grid(square(16), boxes);
  REQUIRE(grid.clearance({4, 4}) == 0);
  REQUIRE(grid.clearance({7, 7}) == 0);
  REQUIRE(grid.clearance({8, 8}) == 1);
}

TEST_CASE("boxes outside the height band do not block") {
  const std::array boxes{CollisionBox{{1.0F, 1.0F, 2.0F}, {2.0F, 2.0F, 3.0F}},
                         CollisionBox{{1.0F, 1.0F, 0.0F}, {2.0F, 2.0F, 0.0F}}};
  const NavGrid grid(square(16), boxes);
  REQUIRE(grid.clearance({5, 5}) == 255);
}

TEST_CASE("a one-tile gap between props has room for a player-sized walker") {
  const std::array boxes{CollisionBox{{0.0F, 0.0F, 0.0F}, {1.0F, 4.0F, 1.0F}},
                         CollisionBox{{2.0F, 0.0F, 0.0F}, {3.0F, 4.0F, 1.0F}}};
  const NavGrid grid(square(16), boxes);
  const uint8_t player = grid.requiredClearance(0.3F);

  REQUIRE(player == 2);
  REQUIRE(grid.isOpen({5, 8}, player));
  REQUIRE(grid.isOpen({6, 8}, player));
  REQUIRE_FALSE(grid.isOpen({4, 8}, player));
  REQUIRE_FALSE(grid.isOpen({7, 8}, player));
}

TEST_CASE("the clearance a radius needs is at least one") {
  const NavGrid grid(square(4), {});
  REQUIRE(grid.requiredClearance(0.0F) == 1);
  REQUIRE(grid.requiredClearance(0.1F) == 1);
  REQUIRE(grid.requiredClearance(0.5F) == 3);
}

TEST_CASE("cells and points convert both ways") {
  const NavGridSpec spec{.origin = {-2.0F, 3.0F}, .width = 8, .height = 8};
  const NavGrid grid(spec, {});

  REQUIRE(grid.cellAt({-2.0F, 3.0F}) == GridCell{0, 0});
  REQUIRE(grid.cellAt({-1.1F, 3.3F}) == GridCell{3, 1});
  REQUIRE_FALSE(grid.cellAt({-2.1F, 3.0F}).has_value());
  REQUIRE_FALSE(grid.cellAt({0.0F, 5.0F}).has_value());
  REQUIRE(grid.centre({3, 1}).x == -1.125F);
  REQUIRE(grid.centre({3, 1}).y == 3.375F);
  REQUIRE(grid.cellOf(grid.indexOf({5, 6})) == GridCell{5, 6});
}

TEST_CASE("cells off the grid are never open") {
  const NavGrid grid(square(4), {});
  REQUIRE_FALSE(grid.contains({-1, 0}));
  REQUIRE_FALSE(grid.isOpen({4, 0}, 1));
  REQUIRE(grid.isOpen({3, 3}, 1));
}
