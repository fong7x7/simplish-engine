#pragma once

/// @file gui-quad-shading.h
/// @brief The GUI fragment shader's shape maths on the CPU: what the
/// software rasterizer paints, and the reference each backend's shader
/// mirrors.
/// @par Threading
/// Pure functions.

#include <engine/gui/gui-vertex.h>

namespace eng {

/// One pixel's colour and coverage, channels in [0, 1], alpha already
/// multiplied by how much of the pixel the shape covers.
struct GuiShadedPixel {
  /// Red.
  float r = 0.0f;
  /// Green.
  float g = 0.0f;
  /// Blue.
  float b = 0.0f;
  /// Alpha times coverage.
  float a = 0.0f;
};

/// Shade the pixel at (@p px, @p py) of the shape quad @p quad — any of
/// its corners, whose shared fields describe it — in the quad's own
/// pixels from its centre, y down: the fill or gradient, then the
/// coverage of its rounded rect, border ring or shadow, as
/// `gui_fs_main` does. Anti-aliased over a pixel either side of an edge.
/// Colours blend in their stored sRGB encoding, where the GPU blends
/// linear light, so gradient midpoints differ slightly.
[[nodiscard]] GuiShadedPixel shadeGuiShape(const GuiVertex& quad, float px,
                                           float py);

}  // namespace eng
