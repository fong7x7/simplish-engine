#pragma once

/// @file path-finder.h
/// @brief Shortest paths across a navigation grid, the same on every machine.
/// @par Threading
/// One instance per thread: a search writes the finder's scratch memory.

#include <array>
#include <cstdint>
#include <engine/spatial/grid-cell.h>
#include <engine/spatial/grid-step.h>
#include <engine/spatial/nav-grid.h>
#include <engine/spatial/path-open-list.h>
#include <engine/spatial/path-request.h>
#include <engine/spatial/path-result.h>
#include <optional>
#include <span>
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
/// **Cheap per cell.** The open list (`PathOpenList`) buckets cells by
/// estimated total, each bucket a small heap of one-number keys. A cell
/// whose cost improves is pushed again rather than moved, and its older
/// entry is skipped when it comes first: the order is total and an improved
/// entry always comes before the old one, so cells are expanded in exactly
/// the order a single heap that moved them would give. A cell's mark and
/// cost share one word, neighbours are read straight from the grid's
/// clearances, and the heuristic comes from coordinates already in hand.
///
/// **No allocation per search.** Scratch memory is sized to the grid once
/// and reused; the finder grows only if handed a larger grid than it has
/// seen, which a caller does outside a tick. Per-cell state is stamped with
/// a search number rather than cleared, so starting a search costs nothing
/// in the size of the grid. The open list's buckets grow to the most a
/// search has needed and keep that, so a search that needs more than any
/// before it grows them once.
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
  /// Expand open cells until the goal is closed, none is left, or the
  /// request's budget is spent.
  [[nodiscard]] PathResult search(const NavGrid& grid,
                                  const PathRequest& request);
  /// Close @p index and offer every neighbour a step from it may reach.
  void expand(uint32_t index);
  /// The steps a walker at @p index, in cell @p at, may take: bit `d` set
  /// for `GRID_STEPS[d]`. Away from the grid's edge, each straight
  /// neighbour is read once and a diagonal reuses the two beside it.
  [[nodiscard]] uint32_t stepsFrom(uint32_t index, GridCell at) const;
  /// `stepsFrom` for a cell on the grid's edge, where a step may leave it.
  [[nodiscard]] uint32_t stepsAtEdge(GridCell at) const;
  /// Whether the cell a step in direction @p dir from @p index reaches is
  /// open; the step must stay on the grid.
  [[nodiscard]] bool openAt(uint32_t index, uint32_t dir) const;
  /// Whether a walker in @p at may take @p step: onto an open cell of the
  /// grid and, for a diagonal, past two open cells beside it.
  [[nodiscard]] bool canStep(GridCell at, const GridStep& step) const;
  /// Offer the cell at @p to, which is @p cell, a path costing @p cost;
  /// whether it was better than any the cell had.
  bool offer(uint32_t to, uint32_t cost, GridCell cell);
  /// The octile distance from @p cell to the goal.
  [[nodiscard]] uint32_t heuristic(GridCell cell) const;
  /// Take the next cell to expand off the open list, skipping entries for
  /// cells already closed; nothing when none is left.
  std::optional<uint32_t> nextOpen();
  /// Whether the current search has closed the cell at @p index.
  [[nodiscard]] bool closed(uint32_t index) const;
  /// The mark of a cell the current search has reached and not closed.
  [[nodiscard]] uint32_t openMark() const;
  /// The state of a cell the current search reached for @p cost, open.
  [[nodiscard]] uint64_t openState(uint32_t cost) const;
  /// The best cost the current search has found to the cell at @p index.
  [[nodiscard]] uint32_t costOf(uint32_t index) const;
  /// The path from the start to @p goal, into the path buffer.
  [[nodiscard]] std::span<const GridCell> trace(const NavGrid& grid,
                                                uint32_t goal);

  /// Each cell's state, in one word so a neighbour is judged with one
  /// read: in the high half its mark — twice the number of the search that
  /// last reached it while it is open, one more once it is closed — and in
  /// the low half the cost of the best path to it that search found.
  std::vector<uint64_t> state_;
  /// The direction of the step that reached each cell, to trace back.
  std::vector<uint8_t> via_;
  /// The cells reached and not yet expanded; entries for cells since closed
  /// are skipped.
  PathOpenList open_;
  /// The last path found, start first.
  std::vector<GridCell> path_;
  /// The current search's grid clearances, row-major.
  std::span<const uint8_t> cells_{};
  /// The current search's number, below 2³¹ so twice it fits a mark; 0 is
  /// never used, so fresh scratch is never mistaken for part of a search.
  uint32_t search_ = 0;
  /// The grid width, for turning an index back into a cell.
  uint32_t width_ = 0;
  /// The grid height.
  uint32_t height_ = 0;
  /// The clearance every cell of the current search's path must have.
  uint8_t need_ = 1;
  /// How far each of `GRID_STEPS` moves along the current grid's row-major
  /// order.
  std::array<int32_t, GRID_STEPS.size()> offsets_{};
  /// The goal of the current search.
  GridCell goal_{};
  /// The row-major index of the current search's goal.
  uint32_t goal_index_ = 0;
  /// The row-major index of the current search's start.
  uint32_t start_ = 0;
};

}  // namespace eng::spatial
