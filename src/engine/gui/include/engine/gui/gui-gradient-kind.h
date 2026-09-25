#pragma once

/// @file gui-gradient-kind.h
/// @brief Which way a gradient runs.
/// @par Threading
/// Constants only.

#include <cstdint>

namespace eng {

/// The shape of a gradient.
enum class GuiGradientKind : uint8_t {
  /// Along a line at the gradient's angle, edge to edge.
  LINEAR,
  /// Out from the centre to the edges, an ellipse fitted to the rect.
  RADIAL,
};

}  // namespace eng
