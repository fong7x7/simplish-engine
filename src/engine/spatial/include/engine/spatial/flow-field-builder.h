#pragma once

/// @file flow-field-builder.h
/// @brief Building a flow field a little at a time, so no tick pays for a
/// whole one.
/// @par Threading
/// One instance per thread: building writes the builder's own field.

#include <cstdint>
#include <engine/spatial/flow-field.h>
#include <engine/spatial/grid-cell.h>
#include <engine/spatial/nav-grid.h>
#include <vector>

namespace eng::spatial {

/// Dijkstra's search outward from a goal, resumable: `start` sets it up and
/// each `advance` expands at most so many cells, so a field over a large
/// level is spread across ticks at a fixed cost each.
///
/// **Deterministic to the bit.** The open set is a heap keyed on cost,
/// then row-major index — a total order, so the cells come off it in the
/// same order on every machine and every standard library, and the work a
/// given number of `advance` calls has done is the same everywhere. The
/// finished costs are shortest distances, which are unique anyway.
///
/// **No allocation per search, nearly.** The field and the heap are sized
/// to the grid once. A cell can be pushed more than once as better ways to
/// it are found; the heap is reserved for twice the cells, which a level
/// of rooms and corridors does not exceed, and grows only if one does.
class FlowFieldBuilder {
public:
  /// A builder with scratch for grids of @p cells cells.
  explicit FlowFieldBuilder(uint32_t cells = 0);

  /// Begin a field toward @p goal on @p grid for characters needing
  /// @p clearance. A goal that is not open for them gives a field that is
  /// complete at once, reaching nothing.
  void start(const NavGrid& grid, GridCell goal, uint8_t clearance);
  /// Expand at most @p budget cells; true when the field is complete.
  bool advance(const NavGrid& grid, uint32_t budget);

  /// Whether a field has been started and is not complete yet.
  [[nodiscard]] bool building() const;
  /// How many cells have come off the heap since `start` — how far the
  /// search has got, which is all it takes to say what state it is in.
  [[nodiscard]] uint32_t expanded() const { return expanded_; }
  /// The field being built, or last built.
  [[nodiscard]] FlowField& field() { return field_; }
  /// The field being built, or last built.
  [[nodiscard]] const FlowField& field() const { return field_; }

private:
  /// Offer every neighbour of the cell at @p index a walk through it.
  void expand(const NavGrid& grid, uint32_t index, uint32_t cost);
  /// Put the cell at @p index on the heap at @p cost.
  void push(uint32_t index, uint32_t cost);

  /// The field the search writes.
  FlowField field_;
  /// Each cell's best cost so far, while building; `FLOW_UNREACHED` for
  /// none yet.
  std::vector<uint32_t> best_;
  /// The open cells, as (cost << 32 | index), smallest first.
  std::vector<uint64_t> heap_;
  /// Cells taken off the heap since `start`.
  uint32_t expanded_ = 0;
};

}  // namespace eng::spatial
