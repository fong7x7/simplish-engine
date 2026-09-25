#pragma once

/// @file water-layer.h
/// @brief Water painted over a level's ground: its own layer, cell by cell.
/// @par Threading Main-thread-only (owns heap storage).

#include <engine/math/vec2.h>
#include <engine/render-ground/ground-cell.h>
#include <engine/render-ground/ground-grid.h>
#include <engine/render-ground/ground-rect.h>
#include <engine/render-water/water-cell.h>

namespace eng {

/// Water, laid over the ground rather than painted as one of its
/// terrains: the grass, sand or road a cell holds stays as it is under
/// whatever water lies over it, and shows through as far as the water is
/// clear.
///
/// Eight grids of bytes over the same cells, one per field of `WaterCell`,
/// each growing to hold whatever is painted as a `GroundGrid` does. A cell
/// is water where `depth` is not zero; the others are read only there.
struct WaterLayer {
  /// Each cell's depth in `WATER_DEPTH_STEP`s; 0 is dry.
  GroundGrid depth;
  /// Each cell's sRGB red.
  GroundGrid red;
  /// Each cell's sRGB green.
  GroundGrid green;
  /// Each cell's sRGB blue.
  GroundGrid blue;
  /// Each cell's opacity, 0 to 255.
  GroundGrid opacity;
  /// Which way each cell flows, in 256ths of a turn from east.
  GroundGrid flow_heading;
  /// How fast each cell flows, in 255ths of `WATER_MAX_FLOW_SPEED`.
  GroundGrid flow_speed;
  /// How thick each cell's water is, 0 to 255.
  GroundGrid viscosity;

  /// Two layers are equal when every grid is.
  bool operator==(const WaterLayer&) const = default;
};

/// The water on @p cell: a dry cell reads as depth 0 and the rest zero.
[[nodiscard]] WaterCell waterCellAt(const WaterLayer& layer, GroundCell cell);

/// Lay @p water on @p cell — or dry it, when its depth is 0, which clears
/// every byte. Returns whether anything changed.
bool setWaterCell(WaterLayer& layer, GroundCell cell, const WaterCell& water);

/// Which way and how fast @p water flows, in tiles a second.
[[nodiscard]] Vec2 waterCellFlow(const WaterCell& water);

/// The smallest rectangle holding every cell of water; empty when dry.
[[nodiscard]] GroundRect waterLayerBounds(const WaterLayer& layer);

}  // namespace eng
