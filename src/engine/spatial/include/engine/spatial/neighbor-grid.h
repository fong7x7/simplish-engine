#pragma once

/// @file neighbor-grid.h
/// @brief Which points are near a point, without testing every pair.
/// @par Threading
/// One instance per thread: `rebuild` writes it, queries only read.

#include <array>
#include <cstdint>
#include <engine/math/vec2.h>
#include <engine/spatial/nav-grid-spec.h>
#include <span>
#include <vector>

namespace eng::spatial {

/// The side of one neighbour bucket, in tiles: about two actors across, so
/// the buckets a separation query touches hold few points it then throws
/// away.
inline constexpr float NEIGHBOR_BUCKET_TILES = 1.0F;

/// A uniform grid of buckets over a level's floor, holding the indices of a
/// set of points — the spatial hash of Engine REQUIREMENTS §6, as a grid
/// rather than a hash because the floor is bounded.
///
/// **Rebuilt, not updated.** `rebuild` sorts every point into its bucket by
/// counting sort each time it is called: a count per bucket, a prefix sum,
/// then the points placed in index order. So the order within a bucket is
/// ascending point index, and a query visits buckets row by row — nothing
/// about the result depends on anything but the points, which is what lets
/// the simulation sum pushes in the order it visits them.
///
/// **No allocation in a tick.** The buckets are sized at construction and
/// the point arrays reserved for the capacity asked for.
///
/// A point off the floor goes in the nearest bucket on its edge, so it is
/// still found by any query that reaches that far.
class NeighborGrid {
public:
  /// A grid of one bucket, holding everything: every query visits every
  /// point.
  NeighborGrid();

  /// Buckets over the floor @p spec covers, for up to @p capacity points.
  NeighborGrid(const NavGridSpec& spec, uint32_t capacity);

  /// Sort @p points into their buckets, replacing whatever was there.
  void rebuild(std::span<const Vec2> points);

  /// Call @p visit with the index of every point in a bucket within
  /// @p radius of @p at, bucket by bucket, row by row, ascending within
  /// each. A superset of the points within @p radius: the caller measures.
  template <typename Visit>
  void forEachNear(Vec2 at, float radius, Visit visit) const {
    const std::array<uint32_t, 4> range = bucketRange(at, radius);
    for (uint32_t y = range[1]; y <= range[3]; ++y) {
      for (uint32_t x = range[0]; x <= range[2]; ++x) {
        const uint32_t bucket = y * width_ + x;
        for (uint32_t k = start_[bucket]; k < start_[bucket + 1]; ++k) {
          visit(entries_[k]);
        }
      }
    }
  }

private:
  /// The bucket column or row @p offset tiles from the origin falls in,
  /// held to the grid's @p count of them.
  [[nodiscard]] uint32_t bucketAlong(float offset, uint32_t count) const;
  /// The bucket @p point falls in.
  [[nodiscard]] uint32_t bucketOf(Vec2 point) const;
  /// The first and last bucket column and row within @p radius of @p at:
  /// x0, y0, x1, y1.
  [[nodiscard]] std::array<uint32_t, 4> bucketRange(Vec2 at,
                                                    float radius) const;

  /// World X and Y of the corner of bucket (0, 0).
  Vec2 origin_{};
  /// Bucket columns.
  uint32_t width_ = 1;
  /// Bucket rows.
  uint32_t height_ = 1;
  /// Where each bucket's points start in `entries_`; one more entry than
  /// there are buckets, the last being the point count.
  std::vector<uint32_t> start_;
  /// Where the next point of each bucket goes, while rebuilding.
  std::vector<uint32_t> fill_;
  /// The bucket each point fell in, while rebuilding.
  std::vector<uint32_t> bucket_;
  /// Point indices, bucket by bucket.
  std::vector<uint32_t> entries_;
};

}  // namespace eng::spatial
