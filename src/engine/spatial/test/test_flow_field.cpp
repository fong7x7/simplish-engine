#include <array>
#include <catch2/catch_test_macros.hpp>
#include <engine/spatial/flow-field-builder.h>
#include <engine/spatial/path-finder.h>
#include <vector>

using eng::physics::CollisionBox;
using eng::spatial::FLOW_UNREACHED;
using eng::spatial::FlowField;
using eng::spatial::FlowFieldBuilder;
using eng::spatial::GridCell;
using eng::spatial::NavGrid;
using eng::spatial::NavGridSpec;
using eng::spatial::PathFinder;
using eng::spatial::PathStatus;

namespace {

/// A grid of 24 × 24 quarter-tile cells from the world origin.
NavGridSpec square() {
  return {.origin = {0.0F, 0.0F}, .width = 24, .height = 24};
}

/// Two walls with a gap in each, at opposite ends, so walks wind.
std::vector<CollisionBox> maze() {
  return {{{2.0F, 0.0F, 0.0F}, {2.25F, 5.0F, 1.0F}},
          {{4.0F, 1.0F, 0.0F}, {4.25F, 6.0F, 1.0F}}};
}

/// A complete field toward @p goal on @p grid, built @p budget cells at a
/// time.
FlowField build(const NavGrid& grid, GridCell goal, uint32_t budget) {
  FlowFieldBuilder builder(grid.cellCount());
  builder.start(grid, goal, 1);
  while (!builder.advance(grid, budget)) {
  }
  return builder.field();
}

}  // namespace

TEST_CASE("a field's costs are the costs of the shortest paths to its goal") {
  const NavGrid grid(square(), maze());
  const GridCell goal{22, 3};
  const FlowField field = build(grid, goal, 1000000);
  PathFinder finder(grid.cellCount());

  for (const GridCell from :
       {GridCell{0, 0}, GridCell{1, 20}, GridCell{12, 1}, GridCell{20, 23}}) {
    const auto path = finder.find(grid, {from, goal, 1});
    REQUIRE(path.status == PathStatus::FOUND);
    REQUIRE(field.cost(grid.indexOf(from)) == path.cost);
  }
  REQUIRE(field.cost(grid.indexOf(goal)) == 0);
}

TEST_CASE("a field built a little at a time is the field built at once") {
  const NavGrid grid(square(), maze());
  const FlowField whole = build(grid, {5, 5}, 1000000);
  const FlowField pieces = build(grid, {5, 5}, 7);
  for (uint32_t i = 0; i < grid.cellCount(); ++i) {
    REQUIRE(pieces.cost(i) == whole.cost(i));
  }
}

TEST_CASE("advancing stops at the budget, and the field is not complete") {
  const NavGrid grid(square(), maze());
  FlowFieldBuilder builder(grid.cellCount());
  builder.start(grid, {5, 5}, 1);
  REQUIRE_FALSE(builder.advance(grid, 10));
  REQUIRE(builder.building());
  REQUIRE(builder.expanded() == 10);
  REQUIRE_FALSE(builder.field().complete());
}

TEST_CASE("a cell walled off from the goal is unreached") {
  // A box round the corner cells (0..3, 0..3), sealing them in.
  const std::vector<CollisionBox> walls{
      {{1.0F, 0.0F, 0.0F}, {1.25F, 1.25F, 1.0F}},
      {{0.0F, 1.0F, 0.0F}, {1.25F, 1.25F, 1.0F}}};
  const NavGrid grid(square(), walls);
  const FlowField field = build(grid, {20, 20}, 1000000);
  REQUIRE(field.cost(grid.indexOf({1, 1})) == FLOW_UNREACHED);
  REQUIRE_FALSE(field.descend(grid, {1, 1}, 4).has_value());
}

TEST_CASE("a goal nobody can stand on gives a field reaching nothing") {
  const NavGrid grid(square(), maze());
  FlowFieldBuilder builder(grid.cellCount());
  builder.start(grid, {8, 2}, 1);
  REQUIRE(builder.field().complete());
  REQUIRE(builder.field().cost(grid.indexOf({0, 0})) == FLOW_UNREACHED);
}

TEST_CASE("descending walks down the field, one legal step at a time") {
  const NavGrid grid(square(), maze());
  const GridCell goal{22, 3};
  const FlowField field = build(grid, goal, 1000000);
  GridCell at{0, 0};
  uint32_t previous = field.cost(grid.indexOf(at));
  for (int step = 0; step < 200 && !(at == goal); ++step) {
    const auto next = field.descend(grid, at, 1);
    REQUIRE(next.has_value());
    REQUIRE(field.cost(grid.indexOf(*next)) < previous);
    previous = field.cost(grid.indexOf(*next));
    at = *next;
  }
  REQUIRE(at == goal);
  REQUIRE(field.descend(grid, goal, 3) == goal);
}
