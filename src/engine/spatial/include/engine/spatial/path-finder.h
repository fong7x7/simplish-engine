#pragma once

/// @file path-finder.h
/// @brief Shortest paths across a navigation grid, the same on every machine.
/// @par Threading
/// One instance per thread: a search writes the finder's scratch memory.

#include <cstdint>
#include <engine/spatial/grid-cell.h>
#include <engine/spatial/nav-grid.h>
#include <engine/spatial/path-request.h>
#include <engine/spatial/path-result.h>
#include <vector>

namespace eng::spatial {

/// A* over a `NavGrid`, for the few characters with somewhere particular to
/// go (Game REQUIREMENTS §5.2: individual pathfinding is for the ones that
/// need a specific route; a crowd shares flow fields).
///
/// **Deterministic to the bit.** Costs are integers — 10 for a step along an
/// axis, 14 for a diagonal — and the heuristic is the matching octile
/// distance, so no float ever decides an ordering. When two open cells tie,
/// the one with the smaller estimated remainder is expanded first, and when
/// that ties too, the one with the smaller row-major index. Neighbours are
/// visited in a fixed order. The same grid and request give the same path
/// on every machine.
///
/// **No corner cutting.** A diagonal step is allowed only when both cells
/// beside it are open too, so a character never clips the corner of a prop
/// it rounds.
///
/// **No allocation per search.** Scratch memory is sized to the grid once
/// and reused; the finder grows only if handed a larger grid than it has
/// seen, which a caller does outside a tick. Per-cell state is stamped with
/// a search number rather than cleared, so starting a search costs nothing
/// in the size of the grid.
class PathFinder {
public:
  /// A finder with no scratch yet; it sizes itself on its first search.
  PathFinder() = default;
  /// A finder with scratch for grids of up to @p cell_count cells.
  explicit PathFinder(uint32_t cell_count);

  /// The shortest path @p request asks for across @p grid.
  [[nodiscard]] PathResult find(const NavGrid& grid,
                                const PathRequest& request);

private:
  /// Grow the scratch to @p cells if it is smaller, and number a new search.
  void newSearch(uint32_t cells);
  /// Size the scratch for @p grid, open a new search, and push its start.
  void begin(const NavGrid& grid, const PathRequest& request);
  /// Expand open cells until the goal is closed, the heap empties, or the
  /// request's budget is spent.
  [[nodiscard]] PathResult search(const NavGrid& grid,
                                  const PathRequest& request);
  /// Close @p index and relax every neighbour a step from it may reach.
  void expand(const NavGrid& grid, uint32_t index, uint8_t clearance);
  /// Offer @p to a path through @p from by the step in direction @p dir.
  void relax(uint32_t from, uint32_t to, uint8_t dir);
  /// The octile distance from the cell at @p index to the goal.
  [[nodiscard]] uint32_t heuristic(uint32_t index) const;
  /// Whether the open cell @p a is expanded before the open cell @p b.
  [[nodiscard]] bool before(uint32_t a, uint32_t b) const;
  /// Put @p index on the open heap.
  void push(uint32_t index);
  /// Take the first open cell off the heap.
  uint32_t pop();
  /// Move the heap entry at @p slot up until its parent comes first.
  void siftUp(uint32_t slot);
  /// Move the heap entry at @p slot down until its children come after.
  void siftDown(uint32_t slot);
  /// Put @p index at heap position @p slot.
  void place(uint32_t slot, uint32_t index);
  /// The path from the start to @p goal, into the path buffer.
  [[nodiscard]] std::span<const GridCell> trace(const NavGrid& grid,
                                                uint32_t goal);

  /// Cost of the best path found so far to each cell.
  std::vector<uint32_t> cost_;
  /// Each cell's cost plus its heuristic: the heap's key.
  std::vector<uint32_t> total_;
  /// The direction of the step that reached each cell, to trace back.
  std::vector<uint8_t> via_;
  /// The search in which each cell's cost was last set.
  std::vector<uint32_t> seen_;
  /// The search in which each cell was last closed.
  std::vector<uint32_t> done_;
  /// Where each open cell sits in the heap.
  std::vector<uint32_t> slot_;
  /// The open cells, as a binary heap ordered by `before`.
  std::vector<uint32_t> heap_;
  /// The last path found, start first.
  std::vector<GridCell> path_;
  /// The current search's number; 0 is never used, so fresh scratch is
  /// never mistaken for part of a search.
  uint32_t search_ = 0;
  /// The grid width, for turning an index back into a cell.
  uint32_t width_ = 0;
  /// The goal of the current search.
  GridCell goal_{};
  /// The row-major index of the current search's goal.
  uint32_t goal_index_ = 0;
  /// The row-major index of the current search's start.
  uint32_t start_ = 0;
};

}  // namespace eng::spatial
