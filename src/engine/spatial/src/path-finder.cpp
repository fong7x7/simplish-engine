#include <algorithm>
#include <array>
#include <cstdlib>
#include <engine/spatial/path-finder.h>

namespace eng::spatial {

namespace {

  /// Cost of a step along an axis.
  constexpr uint32_t STRAIGHT_COST = 10;
  /// Cost of a diagonal step: √2 × 10, rounded, which keeps the octile
  /// heuristic consistent so no cell is ever closed twice.
  constexpr uint32_t DIAGONAL_COST = 14;

  /// One of the eight steps a path may take from a cell.
  struct Step {
    /// Columns moved.
    int32_t dx = 0;
    /// Rows moved.
    int32_t dy = 0;
    /// What the step costs.
    uint32_t cost = 0;
  };

  /// The steps, in the order neighbours are visited: the four along the
  /// axes first, then the four diagonals. The order is part of the
  /// determinism contract — it decides which of two equal paths is found.
  constexpr std::array<Step, 8> STEPS{{{1, 0, STRAIGHT_COST},
                                       {-1, 0, STRAIGHT_COST},
                                       {0, 1, STRAIGHT_COST},
                                       {0, -1, STRAIGHT_COST},
                                       {1, 1, DIAGONAL_COST},
                                       {-1, 1, DIAGONAL_COST},
                                       {1, -1, DIAGONAL_COST},
                                       {-1, -1, DIAGONAL_COST}}};

  /// Whether a path may step from @p cell by @p step: onto an open cell,
  /// and, for a diagonal, past two open cells beside it.
  bool canStep(const NavGrid& grid, GridCell cell, const Step& step,
               uint8_t clearance) {
    if (!grid.isOpen({cell.x + step.dx, cell.y + step.dy}, clearance)) {
      return false;
    }
    if (step.dx == 0 || step.dy == 0) {
      return true;
    }
    return grid.isOpen({cell.x + step.dx, cell.y}, clearance) &&
           grid.isOpen({cell.x, cell.y + step.dy}, clearance);
  }

  /// Whether both ends of @p request are cells a path could stand in.
  bool endpointsOpen(const NavGrid& grid, const PathRequest& request) {
    return grid.isOpen(request.from, request.clearance) &&
           grid.isOpen(request.to, request.clearance);
  }

  /// A result with no path.
  PathResult noPath(PathStatus status, uint32_t expanded) {
    return {.status = status, .expanded = expanded, .cost = 0, .cells = {}};
  }

}  // namespace

PathFinder::PathFinder(uint32_t cell_count)
  : cost_(cell_count), total_(cell_count), via_(cell_count), seen_(cell_count),
    done_(cell_count), slot_(cell_count) {
  heap_.reserve(cell_count);
  path_.reserve(cell_count);
}

PathResult PathFinder::find(const NavGrid& grid, const PathRequest& request) {
  if (!endpointsOpen(grid, request)) {
    return noPath(PathStatus::BLOCKED_ENDPOINT, 0);
  }
  begin(grid, request);
  return search(grid, request);
}

PathResult PathFinder::search(const NavGrid& grid, const PathRequest& request) {
  uint32_t expanded = 0;
  while (!heap_.empty() && expanded < request.max_expansions) {
    const uint32_t current = pop();
    ++expanded;
    if (current == goal_index_) {
      return {PathStatus::FOUND, expanded, cost_[current],
              trace(grid, current)};
    }
    expand(grid, current, request.clearance);
  }
  const PathStatus status =
      heap_.empty() ? PathStatus::UNREACHABLE : PathStatus::OVER_BUDGET;
  return noPath(status, expanded);
}

void PathFinder::newSearch(uint32_t cells) {
  if (cost_.size() < cells) {
    *this = PathFinder(cells);
  }
  if (++search_ == 0) {
    // Wrapped after four billion searches: stale stamps could now match.
    std::ranges::fill(seen_, 0U);
    std::ranges::fill(done_, 0U);
    search_ = 1;
  }
}

void PathFinder::begin(const NavGrid& grid, const PathRequest& request) {
  newSearch(grid.cellCount());
  width_ = grid.spec().width;
  goal_ = request.to;
  goal_index_ = grid.indexOf(request.to);
  start_ = grid.indexOf(request.from);
  heap_.clear();
  cost_[start_] = 0;
  total_[start_] = heuristic(start_);
  seen_[start_] = search_;
  push(start_);
}

void PathFinder::expand(const NavGrid& grid, uint32_t index,
                        uint8_t clearance) {
  done_[index] = search_;
  const GridCell cell = grid.cellOf(index);
  for (size_t dir = 0; dir < STEPS.size(); ++dir) {
    const Step& step = STEPS[dir];
    if (canStep(grid, cell, step, clearance)) {
      relax(index, grid.indexOf({cell.x + step.dx, cell.y + step.dy}),
            static_cast<uint8_t>(dir));
    }
  }
}

void PathFinder::relax(uint32_t from, uint32_t to, uint8_t dir) {
  const uint32_t cost = cost_[from] + STEPS[dir].cost;
  const bool open = seen_[to] == search_;
  if (done_[to] == search_ || (open && cost >= cost_[to])) {
    return;
  }
  cost_[to] = cost;
  total_[to] = cost + heuristic(to);
  via_[to] = dir;
  if (open) {
    siftUp(slot_[to]);
  } else {
    seen_[to] = search_;
    push(to);
  }
}

uint32_t PathFinder::heuristic(uint32_t index) const {
  const auto x = static_cast<int32_t>(index % width_);
  const auto y = static_cast<int32_t>(index / width_);
  const auto dx = static_cast<uint32_t>(std::abs(x - goal_.x));
  const auto dy = static_cast<uint32_t>(std::abs(y - goal_.y));
  return STRAIGHT_COST * std::max(dx, dy) +
         (DIAGONAL_COST - STRAIGHT_COST) * std::min(dx, dy);
}

bool PathFinder::before(uint32_t a, uint32_t b) const {
  if (total_[a] != total_[b]) {
    return total_[a] < total_[b];
  }
  const uint32_t rest_a = total_[a] - cost_[a];
  const uint32_t rest_b = total_[b] - cost_[b];
  return rest_a != rest_b ? rest_a < rest_b : a < b;
}

void PathFinder::push(uint32_t index) {
  heap_.push_back(index);
  slot_[index] = static_cast<uint32_t>(heap_.size() - 1);
  siftUp(slot_[index]);
}

uint32_t PathFinder::pop() {
  const uint32_t first = heap_.front();
  const uint32_t last = heap_.back();
  heap_.pop_back();
  if (!heap_.empty()) {
    place(0, last);
    siftDown(0);
  }
  return first;
}

void PathFinder::siftUp(uint32_t slot) {
  const uint32_t index = heap_[slot];
  while (slot > 0) {
    const uint32_t parent = (slot - 1) / 2;
    if (!before(index, heap_[parent])) {
      break;
    }
    place(slot, heap_[parent]);
    slot = parent;
  }
  place(slot, index);
}

void PathFinder::siftDown(uint32_t slot) {
  const uint32_t index = heap_[slot];
  const auto size = static_cast<uint32_t>(heap_.size());
  while (2 * slot + 1 < size) {
    uint32_t child = 2 * slot + 1;
    if (child + 1 < size && before(heap_[child + 1], heap_[child])) {
      ++child;
    }
    if (!before(heap_[child], index)) {
      break;
    }
    place(slot, heap_[child]);
    slot = child;
  }
  place(slot, index);
}

void PathFinder::place(uint32_t slot, uint32_t index) {
  heap_[slot] = index;
  slot_[index] = slot;
}

std::span<const GridCell> PathFinder::trace(const NavGrid& grid,
                                            uint32_t goal) {
  path_.clear();
  uint32_t index = goal;
  while (index != start_) {
    const GridCell cell = grid.cellOf(index);
    path_.push_back(cell);
    const Step& step = STEPS[via_[index]];
    index = grid.indexOf({cell.x - step.dx, cell.y - step.dy});
  }
  path_.push_back(grid.cellOf(start_));
  std::ranges::reverse(path_);
  return path_;
}

}  // namespace eng::spatial
