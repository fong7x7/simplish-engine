#include <algorithm>
#include <array>
#include <cmath>
#include <engine/spatial/nav-grid.h>
#include <utility>

namespace eng::spatial {

namespace {

  /// The clearance every cell starts at before distances are computed: as
  /// far from anything solid as a byte can say.
  constexpr uint8_t CLEARANCE_MAX = 255;

  /// The neighbours the forward sweep has already visited: left, and the
  /// three in the row before.
  constexpr std::array<GridCell, 4> VISITED_BEFORE{
      {{-1, 0}, {-1, -1}, {0, -1}, {1, -1}}};
  /// The neighbours the backward sweep has already visited: right, and the
  /// three in the row after.
  constexpr std::array<GridCell, 4> VISITED_AFTER{
      {{1, 0}, {1, 1}, {0, 1}, {-1, 1}}};

  /// Whether @p box reaches into the height band the grid is for.
  bool blocksFloor(const physics::CollisionBox& box, const NavGridSpec& spec) {
    return box.max.z > spec.floor_z &&
           box.min.z < spec.floor_z + spec.clear_height;
  }

  /// The cells, along one axis of @p count cells, that the span from
  /// @p lo to @p hi overlaps — both in cells from the grid's origin — as the
  /// first and one past the last. A span ending exactly on a cell edge does
  /// not reach into the next cell.
  std::pair<int32_t, int32_t> coveredCells(float lo, float hi, uint32_t count) {
    const auto limit = static_cast<float>(count);
    const float first = std::clamp(std::floor(lo), 0.0F, limit);
    const float end = std::clamp(std::ceil(hi), 0.0F, limit);
    return {static_cast<int32_t>(first), static_cast<int32_t>(end)};
  }

  /// Mark the cells of @p cells that @p box overlaps as solid.
  void markBox(std::span<uint8_t> cells, const NavGridSpec& spec,
               const physics::CollisionBox& box) {
    const float size = spec.cell_size;
    const auto [x0, x1] =
        coveredCells((box.min.x - spec.origin.x) / size,
                     (box.max.x - spec.origin.x) / size, spec.width);
    const auto [y0, y1] =
        coveredCells((box.min.y - spec.origin.y) / size,
                     (box.max.y - spec.origin.y) / size, spec.height);
    for (int32_t y = y0; y < y1; ++y) {
      for (int32_t x = x0; x < x1; ++x) {
        cells[static_cast<size_t>(y) * spec.width + static_cast<size_t>(x)] = 0;
      }
    }
  }

}  // namespace

NavGrid::NavGrid(const NavGridSpec& spec,
                 std::span<const physics::CollisionBox> boxes)
  : spec_(spec),
    clearance_(static_cast<size_t>(spec.width) * spec.height, CLEARANCE_MAX) {
  rasterize(boxes);
  computeClearance();
}

uint32_t NavGrid::cellCount() const {
  return static_cast<uint32_t>(clearance_.size());
}

bool NavGrid::contains(GridCell cell) const {
  return cell.x >= 0 && cell.y >= 0 && std::cmp_less(cell.x, spec_.width) &&
         std::cmp_less(cell.y, spec_.height);
}

std::optional<GridCell> NavGrid::cellAt(Vec2 point) const {
  const float column = std::floor((point.x - spec_.origin.x) / spec_.cell_size);
  const float row = std::floor((point.y - spec_.origin.y) / spec_.cell_size);
  // Compared as floats first, so a point far off the grid — or not a
  // number at all — never reaches an integer conversion it would overflow.
  // Negated rather than distributed: every comparison with NaN is false,
  // so only this form sends a NaN point off the grid.
  // NOLINTNEXTLINE(readability-simplify-boolean-expr): NaN must fail it
  if (!(column >= 0.0F && column < static_cast<float>(spec_.width) &&
        row >= 0.0F && row < static_cast<float>(spec_.height))) {
    return std::nullopt;
  }
  return GridCell{static_cast<int32_t>(column), static_cast<int32_t>(row)};
}

Vec2 NavGrid::centre(GridCell cell) const {
  return {
      spec_.origin.x + (static_cast<float>(cell.x) + 0.5F) * spec_.cell_size,
      spec_.origin.y + (static_cast<float>(cell.y) + 0.5F) * spec_.cell_size};
}

uint32_t NavGrid::indexOf(GridCell cell) const {
  return static_cast<uint32_t>(cell.y) * spec_.width +
         static_cast<uint32_t>(cell.x);
}

GridCell NavGrid::cellOf(uint32_t index) const {
  return {static_cast<int32_t>(index % spec_.width),
          static_cast<int32_t>(index / spec_.width)};
}

uint8_t NavGrid::clearance(GridCell cell) const {
  return contains(cell) ? clearance_[indexOf(cell)] : 0;
}

bool NavGrid::isOpen(GridCell cell, uint8_t required) const {
  return clearance(cell) >= required;
}

uint8_t NavGrid::requiredClearance(float radius) const {
  const float cells = std::ceil(radius / spec_.cell_size + 0.5F);
  return static_cast<uint8_t>(std::clamp(cells, 1.0F, 255.0F));
}

void NavGrid::rasterize(std::span<const physics::CollisionBox> boxes) {
  for (const physics::CollisionBox& box : boxes) {
    if (blocksFloor(box, spec_)) {
      markBox(clearance_, spec_, box);
    }
  }
}

void NavGrid::computeClearance() {
  const auto width = static_cast<int32_t>(spec_.width);
  const auto height = static_cast<int32_t>(spec_.height);
  for (int32_t y = 0; y < height; ++y) {
    for (int32_t x = 0; x < width; ++x) {
      clearance_[indexOf({x, y})] = relaxed({x, y}, VISITED_BEFORE);
    }
  }
  for (int32_t y = height - 1; y >= 0; --y) {
    for (int32_t x = width - 1; x >= 0; --x) {
      clearance_[indexOf({x, y})] = relaxed({x, y}, VISITED_AFTER);
    }
  }
}

uint8_t NavGrid::relaxed(GridCell cell,
                         std::span<const GridCell> neighbours) const {
  uint32_t best = clearance_[indexOf(cell)];
  for (const GridCell offset : neighbours) {
    const GridCell next{cell.x + offset.x, cell.y + offset.y};
    if (contains(next)) {
      best = std::min(best, clearance_[indexOf(next)] + 1U);
    }
  }
  return static_cast<uint8_t>(std::min<uint32_t>(best, CLEARANCE_MAX));
}

}  // namespace eng::spatial
