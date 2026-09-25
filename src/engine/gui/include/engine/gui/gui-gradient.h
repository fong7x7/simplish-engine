#pragma once

/// @file gui-gradient.h
/// @brief A two-colour gradient fill.
/// @par Threading
/// Immutable value type.

#include "gui-color.h"
#include "gui-gradient-kind.h"

namespace eng {

/// A fill from one colour to another. Colours blend in linear light on the
/// GPU, as CSS's do in the default interpolation space.
struct GuiGradient {
  /// Linear or radial.
  GuiGradientKind kind = GuiGradientKind::LINEAR;
  /// At the start: the angle's back, or the centre.
  GuiColor from{};
  /// At the end: the angle's front, or the edge.
  GuiColor to{};
  /// LINEAR: the direction it runs, as CSS's `linear-gradient(<angle>)`:
  /// degrees clockwise from pointing up — 180 runs top to bottom, 90 left
  /// to right.
  float angle_degrees = 180.0f;
};

}  // namespace eng
