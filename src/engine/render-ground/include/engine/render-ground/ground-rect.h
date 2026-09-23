#pragma once

/// @file ground-rect.h
/// @brief A rectangle of ground cells.
/// @par Threading Thread-safe (immutable value type).

#include <cstdint>

namespace eng {

/// The cells from (`x`, `y`) to (`x + width - 1`, `y + height - 1`): what a
/// grid covers, and what a brush or a fill paints.
///
/// A rectangle with no width or no height holds no cells.
/// @thread_safety Immutable value type.
struct GroundRect {
  /// X of the westmost column.
  int32_t x = 0;
  /// Y of the southmost row.
  int32_t y = 0;
  /// How many columns, from `x` east.
  int32_t width = 0;
  /// How many rows, from `y` north.
  int32_t height = 0;

  /// Two rectangles are the same when all four numbers agree.
  bool operator==(const GroundRect&) const = default;
};

}  // namespace eng
