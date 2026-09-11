#include <algorithm>
#include <array>
#include <cmath>
#include <engine/physics/box-broadphase.h>
#include <numeric>
#include <utility>

namespace eng::physics {

namespace {

  /// The smallest and largest X and Y of every footprint in @p boxes.
  std::pair<Vec2, Vec2> footprintBounds(std::span<const CollisionBox> boxes) {
    Vec2 low{boxes[0].min.x, boxes[0].min.y};
    Vec2 high{boxes[0].max.x, boxes[0].max.y};
    for (const CollisionBox& box : boxes) {
      low = {std::min(low.x, box.min.x), std::min(low.y, box.min.y)};
      high = {std::max(high.x, box.max.x), std::max(high.y, box.max.y)};
    }
    return {low, high};
  }

  /// Call @p visit with the index of every cell of a grid @p width wide in
  /// @p range — x0, y0, x1, y1 — row by row.
  template <typename Visit>
  void forEachCell(const std::array<uint32_t, 4>& range, uint32_t width,
                   Visit visit) {
    for (uint32_t y = range[1]; y <= range[3]; ++y) {
      for (uint32_t x = range[0]; x <= range[2]; ++x) {
        visit(y * width + x);
      }
    }
  }

  /// Cells of @p cell tiles needed to cover @p extent tiles: at least one.
  uint32_t cellsFor(float extent, float cell) {
    return std::max(1U, static_cast<uint32_t>(std::ceil(extent / cell)));
  }

}  // namespace

BoxBroadphase::BoxBroadphase(std::span<const CollisionBox> boxes) {
  if (boxes.empty()) {
    return;
  }
  const auto [low, high] = footprintBounds(boxes);
  const float extent = std::max(high.x - low.x, high.y - low.y);
  cell_ = std::max(BOX_BROADPHASE_CELL_TILES,
                   extent / static_cast<float>(BOX_BROADPHASE_MAX_SIDE));
  origin_ = low;
  width_ = cellsFor(high.x - low.x, cell_);
  height_ = cellsFor(high.y - low.y, cell_);
  bucket(boxes);
  capacity_ = static_cast<uint32_t>(entries_.size());
}

void BoxBroadphase::bucket(std::span<const CollisionBox> boxes) {
  start_.assign(static_cast<size_t>(width_) * height_ + 1, 0U);
  for (const CollisionBox& box : boxes) {
    forEachCell(cellsUnder(box), width_,
                [&](uint32_t cell) { ++start_[cell + 1]; });
  }
  std::partial_sum(start_.begin(), start_.end(), start_.begin());
  entries_.resize(start_.back());
  std::vector<uint32_t> fill(start_.begin(), start_.end() - 1);
  for (uint32_t i = 0; i < boxes.size(); ++i) {
    forEachCell(cellsUnder(boxes[i]), width_,
                [&](uint32_t cell) { entries_[fill[cell]++] = i; });
  }
}

std::array<uint32_t, 4>
BoxBroadphase::cellsUnder(const CollisionBox& box) const {
  return {cellAlong(box.min.x - origin_.x, width_),
          cellAlong(box.min.y - origin_.y, height_),
          cellAlong(box.max.x - origin_.x, width_),
          cellAlong(box.max.y - origin_.y, height_)};
}

void BoxBroadphase::gather(Vec2 center, float reach,
                           std::vector<uint32_t>& out) const {
  out.clear();
  if (width_ == 0) {
    return;
  }
  const Vec2 from = center - origin_;
  const std::array<uint32_t, 4> range{
      cellAlong(from.x - reach, width_), cellAlong(from.y - reach, height_),
      cellAlong(from.x + reach, width_), cellAlong(from.y + reach, height_)};
  forEachCell(range, width_, [&](uint32_t cell) {
    out.insert(out.end(), entries_.begin() + start_[cell],
               entries_.begin() + start_[cell + 1]);
  });
  std::ranges::sort(out);
  out.erase(std::ranges::unique(out).begin(), out.end());
}

uint32_t BoxBroadphase::cellAlong(float offset, uint32_t count) const {
  const float cell = std::floor(offset / cell_);
  // Compared as floats first, so a point far off the level — or NaN —
  // cannot overflow the conversion.
  if (!(cell > 0.0F)) {
    return 0;
  }
  const auto last = static_cast<float>(count - 1);
  return cell >= last ? count - 1 : static_cast<uint32_t>(cell);
}

}  // namespace eng::physics
