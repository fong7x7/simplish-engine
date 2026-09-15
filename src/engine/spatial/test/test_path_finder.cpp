#include <array>
#include <catch2/catch_test_macros.hpp>
#include <cstdlib>
#include <engine/core/pcg32.h>
#include <engine/spatial/path-finder.h>
#include <vector>

using eng::physics::CollisionBox;
using eng::spatial::GridCell;
using eng::spatial::NavGrid;
using eng::spatial::NavGridSpec;
using eng::spatial::PathFinder;
using eng::spatial::PathRequest;
using eng::spatial::PathResult;
using eng::spatial::PathStatus;

namespace {

/// A grid of @p side × @p side quarter-tile cells from the world origin.
NavGridSpec square(uint32_t side) {
  return {.origin = {0.0F, 0.0F}, .width = side, .height = side};
}

/// A wall one cell thick at column 8, rows 0 to 13, leaving rows 14 and 15
/// of a 16-cell grid open at the top.
std::array<CollisionBox, 1> wallWithOpening() {
  return {CollisionBox{{2.0F, 0.0F, 0.0F}, {2.25F, 3.5F, 1.0F}}};
}

/// Whether consecutive cells of @p cells are one step apart and no
/// diagonal step clips a cell that is not open.
bool stepsAreLegal(const NavGrid& grid, const std::vector<GridCell>& cells) {
  for (size_t i = 1; i < cells.size(); ++i) {
    const int dx = cells[i].x - cells[i - 1].x;
    const int dy = cells[i].y - cells[i - 1].y;
    if (std::abs(dx) > 1 || std::abs(dy) > 1 || (dx == 0 && dy == 0)) {
      return false;
    }
    if (dx != 0 && dy != 0 &&
        (!grid.isOpen({cells[i - 1].x + dx, cells[i - 1].y}, 1) ||
         !grid.isOpen({cells[i - 1].x, cells[i - 1].y + dy}, 1))) {
      return false;
    }
  }
  return true;
}

/// A 48 × 48 grid strewn with two dozen of @p rng's boxes, from slivers to
/// walls three tiles long.
NavGrid strewn(eng::Pcg32& rng) {
  std::vector<CollisionBox> boxes;
  for (int k = 0; k < 24; ++k) {
    const float x = 12.0F * rng.nextUnitFloat();
    const float y = 12.0F * rng.nextUnitFloat();
    const float w = 0.25F + 3.0F * rng.nextUnitFloat();
    const float h = 0.25F + 3.0F * rng.nextUnitFloat();
    boxes.push_back({{x, y, 0.0F}, {x + w, y + h, 1.0F}});
  }
  return {square(48), boxes};
}

/// One of @p rng's cells of @p grid that a walker needing @p clearance can
/// stand in, drawn again until one is — or the last drawn, after twenty.
GridCell openCell(eng::Pcg32& rng, const NavGrid& grid, uint8_t clearance) {
  GridCell cell{};
  for (int tries = 0; tries < 20; ++tries) {
    cell = {static_cast<int32_t>(rng.nextBelow(48)),
            static_cast<int32_t>(rng.nextBelow(48))};
    if (grid.isOpen(cell, clearance)) {
      break;
    }
  }
  return cell;
}

/// A request across @p grid between two of @p rng's open cells, for one of
/// three widths, every fifth cut short at 60 expansions.
PathRequest anyRequest(eng::Pcg32& rng, const NavGrid& grid, int nth) {
  const auto clearance = static_cast<uint8_t>(1 + rng.nextBelow(3));
  const GridCell from = openCell(rng, grid, clearance);
  return {.from = from,
          .to = openCell(rng, grid, clearance),
          .clearance = clearance,
          .max_expansions =
              nth % 5 == 0 ? 60 : eng::spatial::PATH_DEFAULT_MAX_EXPANSIONS};
}

/// @p digest with @p value folded in, FNV-1a a byte at a time.
uint64_t fold(uint64_t digest, uint64_t value) {
  for (int byte = 0; byte < 8; ++byte) {
    digest ^= (value >> (8 * byte)) & 0xFFU;
    digest *= 0x100000001B3ULL;
  }
  return digest;
}

/// @p digest with everything @p result says folded in.
uint64_t foldResult(uint64_t digest, const PathResult& result) {
  digest = fold(digest, static_cast<uint64_t>(result.status));
  digest = fold(digest, result.expanded);
  digest = fold(digest, result.cost);
  for (const GridCell cell : result.cells) {
    digest = fold(digest, (static_cast<uint64_t>(cell.x) << 32U) |
                              static_cast<uint32_t>(cell.y));
  }
  return digest;
}

}  // namespace

TEST_CASE("a straight path costs ten a step") {
  const NavGrid grid(square(16), {});
  PathFinder finder(grid.cellCount());
  const auto result = finder.find(grid, {.from = {0, 0}, .to = {5, 0}});

  REQUIRE(result.status == PathStatus::FOUND);
  REQUIRE(result.cost == 50);
  REQUIRE(result.cells.size() == 6);
  REQUIRE(result.cells.front() == GridCell{0, 0});
  REQUIRE(result.cells.back() == GridCell{5, 0});
}

TEST_CASE("a diagonal path costs fourteen a step") {
  const NavGrid grid(square(16), {});
  PathFinder finder;
  const auto result = finder.find(grid, {.from = {0, 0}, .to = {3, 3}});
  REQUIRE(result.status == PathStatus::FOUND);
  REQUIRE(result.cost == 42);
  REQUIRE(result.cells.size() == 4);
}

TEST_CASE(
    "a path goes round a wall through its opening, never cutting corners") {
  const auto wall = wallWithOpening();
  const NavGrid grid(square(16), wall);
  PathFinder finder(grid.cellCount());
  const auto result = finder.find(grid, {.from = {2, 2}, .to = {13, 2}});

  REQUIRE(result.status == PathStatus::FOUND);
  const std::vector<GridCell> cells(result.cells.begin(), result.cells.end());
  REQUIRE(stepsAreLegal(grid, cells));
  bool through_opening = false;
  for (const GridCell cell : cells) {
    REQUIRE(grid.isOpen(cell, 1));
    through_opening = through_opening || (cell.x == 8 && cell.y >= 14);
  }
  REQUIRE(through_opening);
}

TEST_CASE("the same request finds the same path, whichever finder runs it") {
  const auto wall = wallWithOpening();
  const NavGrid grid(square(16), wall);
  PathFinder first;
  PathFinder second(grid.cellCount());
  const PathRequest request{.from = {1, 1}, .to = {14, 3}};

  const auto a = first.find(grid, request);
  const std::vector<GridCell> cells(a.cells.begin(), a.cells.end());
  (void)second.find(grid, {.from = {15, 15}, .to = {0, 0}});
  const auto b = second.find(grid, request);

  REQUIRE(std::vector<GridCell>(b.cells.begin(), b.cells.end()) == cells);
  REQUIRE(a.expanded == b.expanded);
}

TEST_CASE("a goal sealed off is unreachable") {
  // A box ring around cells (10..13, 10..13).
  const std::array boxes{
      CollisionBox{{2.25F, 2.25F, 0.0F}, {3.75F, 2.5F, 1.0F}},
      CollisionBox{{2.25F, 3.5F, 0.0F}, {3.75F, 3.75F, 1.0F}},
      CollisionBox{{2.25F, 2.25F, 0.0F}, {2.5F, 3.75F, 1.0F}},
      CollisionBox{{3.5F, 2.25F, 0.0F}, {3.75F, 3.75F, 1.0F}}};
  const NavGrid grid(square(16), boxes);
  PathFinder finder;
  const auto result = finder.find(grid, {.from = {0, 0}, .to = {12, 12}});
  REQUIRE(result.status == PathStatus::UNREACHABLE);
  REQUIRE(result.cells.empty());
}

TEST_CASE("an end in a solid or too-narrow cell is refused before searching") {
  const auto wall = wallWithOpening();
  const NavGrid grid(square(16), wall);
  PathFinder finder;
  REQUIRE(finder.find(grid, {.from = {0, 0}, .to = {8, 3}}).status ==
          PathStatus::BLOCKED_ENDPOINT);
  REQUIRE(finder.find(grid, {.from = {7, 3}, .to = {0, 0}, .clearance = 2})
              .status == PathStatus::BLOCKED_ENDPOINT);
  REQUIRE(finder.find(grid, {.from = {0, 0}, .to = {40, 0}}).status ==
          PathStatus::BLOCKED_ENDPOINT);
}

TEST_CASE("a search that runs out of budget says so") {
  const NavGrid grid(square(16), {});
  PathFinder finder;
  const auto result =
      finder.find(grid, {.from = {0, 0}, .to = {15, 15}, .max_expansions = 3});
  REQUIRE(result.status == PathStatus::OVER_BUDGET);
  REQUIRE(result.expanded == 3);
}

TEST_CASE("a wider walker needs a wider way") {
  // A gap of two cells: open for clearance 1, and too narrow for 2.
  const std::array boxes{
      CollisionBox{{2.0F, 0.0F, 0.0F}, {2.25F, 1.75F, 1.0F}},
      CollisionBox{{2.0F, 2.25F, 0.0F}, {2.25F, 4.0F, 1.0F}}};
  const NavGrid grid(square(16), boxes);
  PathFinder finder;
  REQUIRE(finder.find(grid, {.from = {2, 8}, .to = {13, 8}}).status ==
          PathStatus::FOUND);
  REQUIRE(finder.find(grid, {.from = {2, 8}, .to = {13, 8}, .clearance = 2})
              .status == PathStatus::UNREACHABLE);
}

TEST_CASE("a path from a cell to itself is that cell") {
  const NavGrid grid(square(4), {});
  PathFinder finder;
  const auto result = finder.find(grid, {.from = {1, 2}, .to = {1, 2}});
  REQUIRE(result.status == PathStatus::FOUND);
  REQUIRE(result.cost == 0);
  REQUIRE(result.cells.size() == 1);
}

TEST_CASE("a thousand searches find what they always have, tie for tie") {
  // Which of two equal paths is found, and how many cells it takes to find
  // it, is part of the determinism contract: a replay recorded before a
  // change to the finder must plan the same paths after it.
  eng::Pcg32 rng(11, 3);
  PathFinder finder;
  uint64_t digest = 0xCBF29CE484222325ULL;
  for (int g = 0; g < 40; ++g) {
    const NavGrid grid = strewn(rng);
    for (int s = 0; s < 25; ++s) {
      digest = foldResult(digest, finder.find(grid, anyRequest(rng, grid, s)));
    }
  }
  REQUIRE(digest == 0x44F80D364840F431ULL);
}
