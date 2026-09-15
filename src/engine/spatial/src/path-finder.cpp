#include <algorithm>
#include <array>
#include <bit>
#include <cstdlib>
#include <engine/spatial/path-finder.h>
#include <optional>
#include <utility>

namespace eng::spatial {

namespace {

  /// Whether both ends of @p request are cells a path could stand in.
  bool endpointsOpen(const NavGrid& grid, const PathRequest& request) {
    return grid.isOpen(request.from, request.clearance) &&
           grid.isOpen(request.to, request.clearance);
  }

  /// A result with no path.
  PathResult noPath(PathStatus status, uint32_t expanded) {
    return {.status = status, .expanded = expanded, .cost = 0, .cells = {}};
  }

  /// The straight steps a walker takes in `GRID_STEPS` order before the
  /// diagonals, which follow them.
  constexpr uint32_t STRAIGHT_STEPS = 4;

  /// For each diagonal step, in `GRID_STEPS` order, the two straight steps
  /// beside it: the cells a walker taking it passes between.
  constexpr std::array<std::array<uint32_t, 2>, 4> BESIDE{
      {{0, 2}, {1, 2}, {0, 3}, {1, 3}}};

  /// Whether `BESIDE` names, for each diagonal, the straight steps that
  /// add up to it.
  consteval bool besideMatchesSteps() {
    for (uint32_t d = 0; d < BESIDE.size(); ++d) {
      const GridStep& diagonal = GRID_STEPS[STRAIGHT_STEPS + d];
      const GridStep& a = GRID_STEPS[BESIDE[d][0]];
      const GridStep& b = GRID_STEPS[BESIDE[d][1]];
      if (a.dx + b.dx != diagonal.dx || a.dy + b.dy != diagonal.dy) {
        return false;
      }
    }
    return true;
  }
  static_assert(besideMatchesSteps());

  /// Searches a finder numbers before starting its marks again: twice one
  /// less than this, plus one, still fits a mark's 32 bits.
  constexpr uint32_t MAX_SEARCHES = 1U << 31U;

}  // namespace

PathFinder::PathFinder(uint32_t cell_count)
  : state_(cell_count), via_(cell_count) {
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
  for (auto current = nextOpen(); current; current = nextOpen()) {
    if (expanded == request.max_expansions) {
      return noPath(PathStatus::OVER_BUDGET, expanded);
    }
    ++expanded;
    if (*current == goal_index_) {
      return {PathStatus::FOUND, expanded, costOf(*current),
              trace(grid, *current)};
    }
    expand(*current);
  }
  return noPath(PathStatus::UNREACHABLE, expanded);
}

std::optional<uint32_t> PathFinder::nextOpen() {
  while (!open_.empty()) {
    const uint32_t index = open_.take();
    if (!closed(index)) {
      return index;
    }
  }
  return std::nullopt;
}

void PathFinder::newSearch(uint32_t cells) {
  if (state_.size() < cells) {
    *this = PathFinder(cells);
  }
  if (++search_ == MAX_SEARCHES) {
    // Two billion searches: stale marks could now match.
    std::ranges::fill(state_, 0U);
    search_ = 1;
  }
}

void PathFinder::begin(const NavGrid& grid, const PathRequest& request) {
  newSearch(grid.cellCount());
  cells_ = grid.clearances();
  width_ = grid.spec().width;
  height_ = grid.spec().height;
  need_ = request.clearance;
  for (size_t dir = 0; dir < GRID_STEPS.size(); ++dir) {
    offsets_[dir] = (GRID_STEPS[dir].dy * static_cast<int32_t>(width_)) +
                    GRID_STEPS[dir].dx;
  }
  goal_ = request.to;
  goal_index_ = grid.indexOf(request.to);
  start_ = grid.indexOf(request.from);
  state_[start_] = openState(0);
  const uint32_t rest = heuristic(request.from);
  open_.clear(rest);
  open_.push(rest, rest, start_);
}

void PathFinder::expand(uint32_t index) {
  const uint32_t cost = costOf(index);
  state_[index] = (static_cast<uint64_t>(openMark() + 1) << 32U) | cost;
  const GridCell at{static_cast<int32_t>(index % width_),
                    static_cast<int32_t>(index / width_)};
  for (uint32_t open = stepsFrom(index, at); open != 0; open &= open - 1) {
    const auto dir = static_cast<size_t>(std::countr_zero(open));
    const GridStep& step = GRID_STEPS[dir];
    const uint32_t next = index + static_cast<uint32_t>(offsets_[dir]);
    if (offer(next, cost + step.cost, {at.x + step.dx, at.y + step.dy})) {
      via_[next] = static_cast<uint8_t>(dir);
    }
  }
}

uint32_t PathFinder::stepsFrom(uint32_t index, GridCell at) const {
  if (at.x < 1 || at.y < 1 || std::cmp_greater_equal(at.x + 1, width_) ||
      std::cmp_greater_equal(at.y + 1, height_)) {
    return stepsAtEdge(at);
  }
  uint32_t open = 0;
  for (uint32_t dir = 0; dir < STRAIGHT_STEPS; ++dir) {
    open |= static_cast<uint32_t>(openAt(index, dir)) << dir;
  }
  for (uint32_t d = 0; d < BESIDE.size(); ++d) {
    const uint32_t beside = (open >> BESIDE[d][0]) & (open >> BESIDE[d][1]);
    const uint32_t dir = STRAIGHT_STEPS + d;
    open |= (beside & static_cast<uint32_t>(openAt(index, dir))) << dir;
  }
  return open;
}

uint32_t PathFinder::stepsAtEdge(GridCell at) const {
  uint32_t open = 0;
  for (size_t dir = 0; dir < GRID_STEPS.size(); ++dir) {
    open |= static_cast<uint32_t>(canStep(at, GRID_STEPS[dir])) << dir;
  }
  return open;
}

bool PathFinder::openAt(uint32_t index, uint32_t dir) const {
  return cells_[index + static_cast<uint32_t>(offsets_[dir])] >= need_;
}

bool PathFinder::canStep(GridCell at, const GridStep& step) const {
  const auto x = static_cast<uint32_t>(at.x + step.dx);
  const auto y = static_cast<uint32_t>(at.y + step.dy);
  if (x >= width_ || y >= height_ || cells_[(y * width_) + x] < need_) {
    return false;
  }
  const auto from_x = static_cast<uint32_t>(at.x);
  const auto from_y = static_cast<uint32_t>(at.y);
  return step.dx == 0 || step.dy == 0 ||
         (cells_[(from_y * width_) + x] >= need_ &&
          cells_[(y * width_) + from_x] >= need_);
}

bool PathFinder::offer(uint32_t to, uint32_t cost, GridCell cell) {
  const uint64_t state = state_[to];
  const auto mark = static_cast<uint32_t>(state >> 32U);
  if (mark == openMark() + 1 ||
      (mark == openMark() && cost >= static_cast<uint32_t>(state))) {
    return false;
  }
  state_[to] = openState(cost);
  const uint32_t rest = heuristic(cell);
  open_.push(cost + rest, rest, to);
  return true;
}

uint32_t PathFinder::heuristic(GridCell cell) const {
  const auto dx = static_cast<uint32_t>(std::abs(cell.x - goal_.x));
  const auto dy = static_cast<uint32_t>(std::abs(cell.y - goal_.y));
  return GRID_STRAIGHT_COST * std::max(dx, dy) +
         (GRID_DIAGONAL_COST - GRID_STRAIGHT_COST) * std::min(dx, dy);
}

uint32_t PathFinder::openMark() const {
  return search_ * 2;
}

uint64_t PathFinder::openState(uint32_t cost) const {
  return (static_cast<uint64_t>(openMark()) << 32U) | cost;
}

uint32_t PathFinder::costOf(uint32_t index) const {
  return static_cast<uint32_t>(state_[index]);
}

bool PathFinder::closed(uint32_t index) const {
  return static_cast<uint32_t>(state_[index] >> 32U) == openMark() + 1;
}

std::span<const GridCell> PathFinder::trace(const NavGrid& grid,
                                            uint32_t goal) {
  path_.clear();
  uint32_t index = goal;
  while (index != start_) {
    const GridCell cell = grid.cellOf(index);
    path_.push_back(cell);
    const GridStep& step = GRID_STEPS[via_[index]];
    index = grid.indexOf({cell.x - step.dx, cell.y - step.dy});
  }
  path_.push_back(grid.cellOf(start_));
  std::ranges::reverse(path_);
  return path_;
}

}  // namespace eng::spatial
