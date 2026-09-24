#pragma once

/// @file water-corners.h
/// @brief The water at every cell corner, so it can be read anywhere and
/// blends from one tile to the next.
/// @par Threading Main-thread-only (owns heap storage).

#include <cstdint>
#include <engine/math/vec2.h>
#include <engine/render-ground/ground-cell.h>
#include <engine/render-ground/ground-rect.h>
#include <engine/render-water/water-layer.h>
#include <engine/render-water/water-sample.h>
#include <vector>

namespace eng {

/// The water at each corner of a rectangle of cells: the mean of the
/// water cells meeting there, and dry where none do.
///
/// Read between corners bilinearly (`waterSampleAt`), which is what blends
/// a lake into the pond it opens onto — its depth, and a muddy puddle's
/// colour into a clear stream's — over the tile between them rather than
/// stepping at the cell's edge.
struct WaterCorners {
  /// The corner at the south-west of the rectangle.
  GroundCell origin{};
  /// Corners along each row: one more than the rectangle's width.
  int32_t width = 0;
  /// Rows of corners: one more than its height.
  int32_t height = 0;
  /// Each corner's water, row by row from the south-west.
  std::vector<WaterSample> samples;
};

/// The corners of @p cells, from @p layer.
[[nodiscard]] WaterCorners makeWaterCorners(const WaterLayer& layer,
                                            GroundRect cells);

/// The water at @p at, blended between the four corners round it: depth
/// bilinearly, and colour and opacity weighted by depth as well, so a dry
/// corner pulls the depth down without darkening the colour. Dry outside
/// the rectangle. Not yet shelved towards a bank: that is
/// `waterBankShelf`'s.
[[nodiscard]] WaterSample waterSampleAt(const WaterCorners& corners, Vec2 at);

}  // namespace eng
