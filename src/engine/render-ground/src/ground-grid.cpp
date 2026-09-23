#include <algorithm>
#include <engine/render-ground/ground-grid.h>
#include <utility>

namespace eng {

namespace {

  /// Whether @p cell lies inside @p rect.
  bool contains(const GroundRect& rect, GroundCell cell) {
    return cell.x >= rect.x && cell.y >= rect.y &&
           cell.x < rect.x + rect.width && cell.y < rect.y + rect.height;
  }

  /// Whether every cell of @p rect lies inside the coordinate limit.
  bool withinLimit(const GroundRect& rect) {
    return rect.x >= -GROUND_COORDINATE_LIMIT &&
           rect.y >= -GROUND_COORDINATE_LIMIT &&
           rect.x + rect.width <= GROUND_COORDINATE_LIMIT &&
           rect.y + rect.height <= GROUND_COORDINATE_LIMIT;
  }

  /// @p value rounded down to a whole number of growth blocks.
  int32_t blockFloor(int32_t value) {
    const int32_t below = value < 0 ? value - (GROUND_GROWTH_BLOCK - 1) : value;
    return below / GROUND_GROWTH_BLOCK * GROUND_GROWTH_BLOCK;
  }

  /// The block-aligned rectangle holding @p rect and @p cell.
  GroundRect grownRect(const GroundRect& rect, GroundCell cell) {
    const bool had_cells = rect.width > 0 && rect.height > 0;
    const int32_t x0 =
        blockFloor(had_cells ? std::min(rect.x, cell.x) : cell.x);
    const int32_t y0 =
        blockFloor(had_cells ? std::min(rect.y, cell.y) : cell.y);
    const int32_t x1 =
        had_cells ? std::max(rect.x + rect.width, cell.x + 1) : cell.x + 1;
    const int32_t y1 =
        had_cells ? std::max(rect.y + rect.height, cell.y + 1) : cell.y + 1;
    return {x0, y0, blockFloor(x1 - x0 + GROUND_GROWTH_BLOCK - 1),
            blockFloor(y1 - y0 + GROUND_GROWTH_BLOCK - 1)};
  }

  /// Grow @p rect by one cell on every side it lacks to hold @p cell.
  void enclose(GroundRect& rect, GroundCell cell) {
    if (rect.width == 0) {
      rect = {cell.x, cell.y, 1, 1};
      return;
    }
    const int32_t x1 = std::max(rect.x + rect.width, cell.x + 1);
    const int32_t y1 = std::max(rect.y + rect.height, cell.y + 1);
    rect.x = std::min(rect.x, cell.x);
    rect.y = std::min(rect.y, cell.y);
    rect.width = x1 - rect.x;
    rect.height = y1 - rect.y;
  }

}  // namespace

std::optional<GroundGrid> GroundGrid::fromCells(GroundRect bounds,
                                                std::vector<uint8_t> cells) {
  if (bounds.width < 0 || bounds.height < 0 || !withinLimit(bounds) ||
      cells.size() != static_cast<size_t>(bounds.width) *
                          static_cast<size_t>(bounds.height)) {
    return std::nullopt;
  }
  GroundGrid grid;
  grid.bounds_ = cells.empty() ? GroundRect{} : bounds;
  grid.cells_ = std::move(cells);
  return grid;
}

std::optional<size_t> GroundGrid::slotOf(GroundCell cell) const {
  if (!contains(bounds_, cell)) {
    return std::nullopt;
  }
  return static_cast<size_t>(cell.y - bounds_.y) *
             static_cast<size_t>(bounds_.width) +
         static_cast<size_t>(cell.x - bounds_.x);
}

uint8_t GroundGrid::at(GroundCell cell) const {
  const std::optional<size_t> slot = slotOf(cell);
  return slot ? cells_[*slot] : uint8_t{0};
}

bool GroundGrid::set(GroundCell cell, uint8_t terrain) {
  if (at(cell) == terrain || !withinLimit({cell.x, cell.y, 1, 1})) {
    return false;
  }
  if (!contains(bounds_, cell)) {
    growToInclude(cell);
  }
  const std::optional<size_t> slot = slotOf(cell);
  if (slot) {
    cells_[*slot] = terrain;
  }
  return slot.has_value();
}

void GroundGrid::growToInclude(GroundCell cell) {
  const GroundRect grown = grownRect(bounds_, cell);
  const auto width = static_cast<size_t>(bounds_.width);
  const auto grown_width = static_cast<size_t>(grown.width);
  std::vector<uint8_t> cells(grown_width * static_cast<size_t>(grown.height),
                             0);
  for (int32_t row = 0; row < bounds_.height; ++row) {
    const size_t from = static_cast<size_t>(row) * width;
    const size_t to =
        static_cast<size_t>(bounds_.y + row - grown.y) * grown_width +
        static_cast<size_t>(bounds_.x - grown.x);
    std::copy_n(cells_.begin() + static_cast<ptrdiff_t>(from), width,
                cells.begin() + static_cast<ptrdiff_t>(to));
  }
  bounds_ = grown;
  cells_ = std::move(cells);
}

GroundRect GroundGrid::paintedBounds() const {
  GroundRect painted{};
  size_t slot = 0;
  for (int32_t row = 0; row < bounds_.height; ++row) {
    for (int32_t column = 0; column < bounds_.width; ++column) {
      if (cells_[slot++] != 0) {
        enclose(painted, {bounds_.x + column, bounds_.y + row});
      }
    }
  }
  return painted;
}

bool GroundGrid::empty() const {
  return std::ranges::all_of(cells_,
                             [](uint8_t terrain) { return terrain == 0; });
}

}  // namespace eng
