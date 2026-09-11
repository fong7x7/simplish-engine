#include <algorithm>
#include <engine/spatial/flow-field-builder.h>
#include <engine/spatial/grid-step.h>
#include <functional>

namespace eng::spatial {

namespace {

  /// A heap entry's cost.
  uint32_t costOf(uint64_t entry) {
    return static_cast<uint32_t>(entry >> 32U);
  }

  /// A heap entry's cell index.
  uint32_t indexOf(uint64_t entry) {
    return static_cast<uint32_t>(entry & 0xFFFFFFFFU);
  }

}  // namespace

FlowFieldBuilder::FlowFieldBuilder(uint32_t cells) : best_(cells) {
  heap_.reserve(static_cast<size_t>(cells) * 2);
}

void FlowFieldBuilder::start(const NavGrid& grid, GridCell goal,
                             uint8_t clearance) {
  field_.reset(grid.cellCount(), goal, clearance);
  best_.assign(grid.cellCount(), FLOW_UNREACHED);
  heap_.clear();
  expanded_ = 0;
  if (grid.isOpen(goal, clearance)) {
    push(grid.indexOf(goal), 0);
  } else {
    field_.markComplete();
  }
}

bool FlowFieldBuilder::advance(const NavGrid& grid, uint32_t budget) {
  for (uint32_t spent = 0; spent < budget && !heap_.empty(); ++spent) {
    std::ranges::pop_heap(heap_, std::greater<>{});
    const uint64_t entry = heap_.back();
    heap_.pop_back();
    ++expanded_;
    // A cell pushed again at a better cost leaves its old entry behind;
    // the old one comes off later and is passed over.
    if (costOf(entry) == best_[indexOf(entry)] &&
        field_.cost(indexOf(entry)) == FLOW_UNREACHED) {
      field_.setCost(indexOf(entry), costOf(entry));
      expand(grid, indexOf(entry), costOf(entry));
    }
  }
  if (heap_.empty()) {
    field_.markComplete();
  }
  return field_.complete();
}

bool FlowFieldBuilder::building() const {
  return !heap_.empty() && !field_.complete();
}

void FlowFieldBuilder::expand(const NavGrid& grid, uint32_t index,
                              uint32_t cost) {
  const GridCell cell = grid.cellOf(index);
  for (const GridStep& step : GRID_STEPS) {
    if (!canGridStep(grid, cell, step, field_.clearance())) {
      continue;
    }
    const uint32_t next = grid.indexOf({cell.x + step.dx, cell.y + step.dy});
    if (cost + step.cost < best_[next]) {
      push(next, cost + step.cost);
    }
  }
}

void FlowFieldBuilder::push(uint32_t index, uint32_t cost) {
  best_[index] = cost;
  heap_.push_back((static_cast<uint64_t>(cost) << 32U) | index);
  std::ranges::push_heap(heap_, std::greater<>{});
}

}  // namespace eng::spatial
