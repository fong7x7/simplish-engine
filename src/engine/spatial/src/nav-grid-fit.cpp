#include <algorithm>
#include <cmath>
#include <engine/spatial/nav-grid-fit.h>
#include <limits>

namespace eng::spatial {

namespace {

  /// An axis-aligned rectangle of the floor, grown to take in points.
  struct FloorExtent {
    /// The corner with the smallest X and Y.
    Vec2 min{std::numeric_limits<float>::max(),
             std::numeric_limits<float>::max()};
    /// The corner with the largest X and Y.
    Vec2 max{std::numeric_limits<float>::lowest(),
             std::numeric_limits<float>::lowest()};
  };

  /// @p extent grown to take in @p point.
  void include(FloorExtent& extent, Vec2 point) {
    extent.min = {std::min(extent.min.x, point.x),
                  std::min(extent.min.y, point.y)};
    extent.max = {std::max(extent.max.x, point.x),
                  std::max(extent.max.y, point.y)};
  }

  /// Cells needed to reach from @p origin past @p far, at most the cap.
  uint32_t cellsSpanning(float origin, float far) {
    const float cells = std::ceil((far - origin) / NAV_CELL_SIZE_TILES);
    return static_cast<uint32_t>(
        std::clamp(cells, 1.0F, static_cast<float>(NAV_GRID_MAX_SIDE_CELLS)));
  }

  /// The rectangle holding the footprint of every box and every point.
  FloorExtent extentOf(std::span<const physics::CollisionBox> boxes,
                       std::span<const Vec2> points) {
    FloorExtent extent;
    for (const physics::CollisionBox& box : boxes) {
      include(extent, {box.min.x, box.min.y});
      include(extent, {box.max.x, box.max.y});
    }
    for (const Vec2 point : points) {
      include(extent, point);
    }
    return extent;
  }

}  // namespace

NavGridSpec fitNavGrid(std::span<const physics::CollisionBox> boxes,
                       std::span<const Vec2> points, float floor_z) {
  const FloorExtent extent = extentOf(boxes, points);
  if (extent.min.x > extent.max.x) {
    return {.origin = {}, .width = 0, .height = 0, .floor_z = floor_z};
  }
  const Vec2 origin{std::floor(extent.min.x - NAV_GRID_MARGIN_TILES),
                    std::floor(extent.min.y - NAV_GRID_MARGIN_TILES)};
  return {
      .origin = origin,
      .width = cellsSpanning(origin.x, extent.max.x + NAV_GRID_MARGIN_TILES),
      .height = cellsSpanning(origin.y, extent.max.y + NAV_GRID_MARGIN_TILES),
      .floor_z = floor_z};
}

}  // namespace eng::spatial
