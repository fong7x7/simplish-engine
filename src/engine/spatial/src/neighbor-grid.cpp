#include <algorithm>
#include <cmath>
#include <engine/spatial/neighbor-grid.h>

namespace eng::spatial {

namespace {

  /// Buckets needed to cover @p cells cells of @p cell_size tiles: at
  /// least one.
  uint32_t bucketsFor(uint32_t cells, float cell_size) {
    const float tiles = static_cast<float>(cells) * cell_size;
    return std::max(
        1U, static_cast<uint32_t>(std::ceil(tiles / NEIGHBOR_BUCKET_TILES)));
  }

}  // namespace

NeighborGrid::NeighborGrid() : start_(2), fill_(1) {}

NeighborGrid::NeighborGrid(const NavGridSpec& spec, uint32_t capacity)
  : origin_(spec.origin), width_(bucketsFor(spec.width, spec.cell_size)),
    height_(bucketsFor(spec.height, spec.cell_size)),
    start_(static_cast<size_t>(width_) * height_ + 1),
    fill_(static_cast<size_t>(width_) * height_) {
  bucket_.reserve(capacity);
  entries_.reserve(capacity);
}

void NeighborGrid::rebuild(std::span<const Vec2> points) {
  std::ranges::fill(start_, 0U);
  bucket_.resize(points.size());
  entries_.resize(points.size());
  for (size_t i = 0; i < points.size(); ++i) {
    bucket_[i] = bucketOf(points[i]);
    ++start_[bucket_[i] + 1];
  }
  for (size_t b = 1; b < start_.size(); ++b) {
    start_[b] += start_[b - 1];
  }
  std::copy(start_.begin(), start_.end() - 1, fill_.begin());
  for (size_t i = 0; i < points.size(); ++i) {
    entries_[fill_[bucket_[i]]++] = static_cast<uint32_t>(i);
  }
}

uint32_t NeighborGrid::bucketAlong(float offset, uint32_t count) const {
  const float bucket = std::floor(offset / NEIGHBOR_BUCKET_TILES);
  // Compared as floats first, so a point far off the floor — or NaN, which
  // fails both tests — cannot overflow the conversion.
  if (!(bucket > 0.0F)) {
    return 0;
  }
  const auto last = static_cast<float>(count - 1);
  return bucket >= last ? count - 1 : static_cast<uint32_t>(bucket);
}

uint32_t NeighborGrid::bucketOf(Vec2 point) const {
  return bucketAlong(point.y - origin_.y, height_) * width_ +
         bucketAlong(point.x - origin_.x, width_);
}

std::array<uint32_t, 4> NeighborGrid::bucketRange(Vec2 at, float radius) const {
  const Vec2 from = at - origin_;
  return {bucketAlong(from.x - radius, width_),
          bucketAlong(from.y - radius, height_),
          bucketAlong(from.x + radius, width_),
          bucketAlong(from.y + radius, height_)};
}

}  // namespace eng::spatial
