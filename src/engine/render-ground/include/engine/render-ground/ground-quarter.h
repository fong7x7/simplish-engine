#pragma once

/// @file ground-quarter.h
/// @brief The four quarters a ground cell is drawn in.
/// @par Threading Thread-safe (immutable value types).

#include <cstdint>

namespace eng {

/// One quarter of a cell, named by the corner it reaches: north is +Y and
/// east is +X.
///
/// A cell is drawn a quarter at a time because each quarter's shape
/// depends on a different three neighbours — the two beside it towards its
/// corner and the one across that corner — which is what lets a road bend
/// round one corner of a tile and run straight past the other three.
/// @thread_safety Immutable value type.
enum class GroundQuarter : uint8_t {
  /// The quarter towards +X, +Y.
  NORTH_EAST,
  /// The quarter towards −X, +Y.
  NORTH_WEST,
  /// The quarter towards −X, −Y.
  SOUTH_WEST,
  /// The quarter towards +X, −Y.
  SOUTH_EAST,
};

/// Every quarter, counter-clockwise from north-east.
inline constexpr GroundQuarter GROUND_QUARTERS[] = {
    GroundQuarter::NORTH_EAST,
    GroundQuarter::NORTH_WEST,
    GroundQuarter::SOUTH_WEST,
    GroundQuarter::SOUTH_EAST,
};

/// Which way along X @p quarter reaches: +1 or −1.
[[nodiscard]] constexpr int32_t groundQuarterDx(GroundQuarter quarter) {
  return quarter == GroundQuarter::NORTH_EAST ||
                 quarter == GroundQuarter::SOUTH_EAST
             ? 1
             : -1;
}

/// Which way along Y @p quarter reaches: +1 or −1.
[[nodiscard]] constexpr int32_t groundQuarterDy(GroundQuarter quarter) {
  return quarter == GroundQuarter::NORTH_EAST ||
                 quarter == GroundQuarter::NORTH_WEST
             ? 1
             : -1;
}

}  // namespace eng
