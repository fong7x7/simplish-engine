#pragma once

/// @file water-cell.h
/// @brief What one cell of water is: how deep, what colour, how clear.
/// @par Threading A value type.

#include <cstdint>

namespace eng {

/// The colour water is painted with when nobody has chosen one, as sRGB
/// bytes: a clear blue-green.
inline constexpr uint8_t WATER_DEFAULT_RED = 46;
inline constexpr uint8_t WATER_DEFAULT_GREEN = 122;
inline constexpr uint8_t WATER_DEFAULT_BLUE = 150;

/// How opaque water is when nobody has chosen, out of 255: clear enough
/// to see a pond's bed, not a lake's.
inline constexpr uint8_t WATER_DEFAULT_OPACITY = 110;

/// The fastest water flows, in tiles a second: a river in spate. A cell's
/// `flow_speed` is out of 255 of it.
inline constexpr float WATER_MAX_FLOW_SPEED = 1.5f;

/// One cell of the water layer, as bytes, the way the level stores it.
///
/// `depth` is in `WATER_DEPTH_STEP`s and 0 means there is no water on the
/// cell; the rest mean nothing then. Opacity is how much of the ground
/// under it a tile of water hides, from crystal clear at 0 to opaque at 255
/// (`water-look.h`); deeper water hides more at any opacity. Water flows
/// the way `flow_heading` says — 256ths of a turn anticlockwise from east
/// — at `flow_speed` 255ths of `WATER_MAX_FLOW_SPEED`: a pond at 0 stands,
/// a river runs. `viscosity` is how thick it is, from water at 0 to
/// something like honey or mud at 255: thick fluid carries ripples slower,
/// smooths them into slow broad swells and stills sooner, and barely raises
/// a wave.
struct WaterCell {
  /// How deep, in `WATER_DEPTH_STEP`s; 0 is dry.
  uint8_t depth = 0;
  /// The water's colour, sRGB red.
  uint8_t red = WATER_DEFAULT_RED;
  /// The water's colour, sRGB green.
  uint8_t green = WATER_DEFAULT_GREEN;
  /// The water's colour, sRGB blue.
  uint8_t blue = WATER_DEFAULT_BLUE;
  /// How opaque it is, 0 to 255.
  uint8_t opacity = WATER_DEFAULT_OPACITY;
  /// Which way it flows, in 256ths of a turn anticlockwise from east.
  uint8_t flow_heading = 0;
  /// How fast it flows, in 255ths of `WATER_MAX_FLOW_SPEED`; 0 stands.
  uint8_t flow_speed = 0;
  /// How thick it is, from water at 0 to the thickest fluid at 255.
  uint8_t viscosity = 0;

  /// Two cells are the same when every byte is.
  bool operator==(const WaterCell&) const = default;
};

}  // namespace eng
