#pragma once

/// @file ground-run.h
/// @brief A run of cells holding the same terrain.
/// @par Threading Thread-safe (immutable value type).

#include <cstdint>

namespace eng {

/// `length` consecutive cells, in row order, all holding `terrain`.
///
/// What a level file stores the ground as: repainting one room changes the
/// runs over that room and leaves the rest of the list alone, which is what
/// makes the file diff by region rather than reflow as a whole.
/// @thread_safety Immutable value type.
struct GroundRun {
  /// The terrain every cell of the run holds.
  uint8_t terrain = 0;
  /// How many cells it covers.
  uint32_t length = 0;

  /// Two runs are equal when both numbers agree.
  bool operator==(const GroundRun&) const = default;
};

}  // namespace eng
