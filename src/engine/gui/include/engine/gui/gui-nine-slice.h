#pragma once

/// @file gui-nine-slice.h
/// @brief An image drawn at any size by stretching only its middle.
/// @par Threading
/// Immutable value type.

#include "layout-edges.h"

#include <cstdint>

namespace eng {

/// A texture cut into nine by `insets`: the corners are drawn at their
/// own size, the edges stretch along one axis, the middle along both —
/// how a framed game panel or a speech bubble scales without warping its
/// border. CSS's `border-image`.
struct GuiNineSlice {
  /// The texture (an `RhiTextureHandle`).
  uint64_t texture = 0;
  /// Its width in texels.
  float texture_w = 0.0f;
  /// Its height in texels.
  float texture_h = 0.0f;
  /// How far in from each edge the corners end, in texels.
  Edges insets{};
  /// Layout pixels per texel for the corners and edges: 2 draws a 16-texel
  /// corner 32 pixels wide.
  float scale = 1.0f;
};

}  // namespace eng
