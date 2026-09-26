#pragma once

/// @file gui-sort-direction.h
/// @brief Which way a table column is sorted.
/// @par Threading
/// Constants only.

#include <cstdint>

namespace eng {

/// The order a sorted column runs in.
enum class GuiSortDirection : uint8_t {
  /// Smallest first.
  ASCENDING,
  /// Largest first.
  DESCENDING,
};

}  // namespace eng
