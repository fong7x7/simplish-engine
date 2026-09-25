#pragma once

/// @file gui-state-style.h
/// @brief What a widget looks like in one interaction state.
/// @par Threading
/// Immutable value type.

#include "gui-color.h"
#include "gui-elevation.h"

namespace eng {

/// A widget's look in one state: its box and its text. Blended between
/// states by `GuiStyleTransition`.
struct GuiStateStyle {
  /// Box fill; zero alpha for none.
  GuiColor fill{0, 0, 0, 0};
  /// Text and icon colour.
  GuiColor text{};
  /// Border colour.
  GuiColor border{0, 0, 0, 0};
  /// Border width in logical pixels; 0 for none.
  float border_width = 0.0f;
  /// Corner radius in logical pixels.
  float radius = 0.0f;
  /// How raised it is, which picks its shadow from the theme.
  GuiElevation elevation = GuiElevation::NONE;

  /// Blend from @p a to @p b by @p t in [0, 1]: colours and sizes
  /// linearly, the elevation switching at halfway.
  static GuiStateStyle lerp(const GuiStateStyle& a, const GuiStateStyle& b,
                            float t);
};

}  // namespace eng
