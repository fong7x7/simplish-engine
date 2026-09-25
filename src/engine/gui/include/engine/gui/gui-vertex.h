#pragma once

/// @file gui-vertex.h
/// @brief One corner of a GUI quad, as every backend's GUI shader reads it.
/// @par Threading
/// Plain data.

#include <cstdint>

namespace eng {

/// `GuiVertex::flags`: sample the bound texture (glyphs, images).
inline constexpr uint32_t GUI_VERTEX_TEXTURED = 0x2u;
/// Cover only the rounded rect `radii` describe, anti-aliased.
inline constexpr uint32_t GUI_VERTEX_SHAPE = 0x4u;
/// Blend `color` to `color2` along the angle in `param`.
inline constexpr uint32_t GUI_VERTEX_LINEAR_GRADIENT = 0x8u;
/// Blend `color` at the centre to `color2` at the edges.
inline constexpr uint32_t GUI_VERTEX_RADIAL_GRADIENT = 0x10u;
/// A soft drop shadow: the shape inset by `param`, faded over `param`.
inline constexpr uint32_t GUI_VERTEX_SHADOW = 0x20u;

/// One corner of a GUI quad. The four corners of a quad carry the same
/// fields except `pos` and `uv`; the fragment shader works in the quad's
/// own pixels, `p = (uv - 0.5) * rect`, so everything below is in layout
/// pixels of the quad as drawn. `gui-vertex-layout.h` states the byte
/// offsets each backend's vertex input restates.
/// @thread_safety Plain data.
struct GuiVertex {
  /// Screen-space position (x, y).
  float pos[2]{};
  /// Texture coordinates, or 0..1 across the quad for shapes.
  float uv[2]{};
  /// Packed RGBA colour; a gradient's start.
  uint32_t color{};
  /// Packed RGBA; a gradient's end.
  uint32_t color2{};
  /// Corner radii: top-left, top-right, bottom-right, bottom-left.
  float radii[4]{};
  /// Border widths: top, right, bottom, left. Any above zero draws only
  /// the ring between the shape and the shape inset by them.
  float border[4]{};
  /// `GUI_VERTEX_*` bits.
  uint32_t flags{};
  /// Quad width in layout pixels; 0 for a plain coloured quad.
  float rect_w{};
  /// Quad height in layout pixels.
  float rect_h{};
  /// A linear gradient's angle in radians (0 runs left to right, a
  /// quarter turn top to bottom), or a shadow's blur in pixels.
  float param{};
};

}  // namespace eng
