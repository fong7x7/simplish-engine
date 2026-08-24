#pragma once

#include <cstdint>

namespace eng {

// --- Vertex format ---

/// @thread_safety Main thread only.
struct GuiVertex {
  /// Screen-space position (x, y).
  float pos[2]{};
  /// Texture coordinates (u, v).
  float uv[2]{};
  /// Packed RGBA colour.
  uint32_t color{};
  /// Corner radius for rounded rectangles.
  float corner_radius{};
  /// Border width in pixels.
  float border_width{};
  /// Flags: `0x1` SDF text, `0x2` textured quad, `0x4` rounded-rect (analytic
  /// coverage in the Metal GUI fragment shader). Atlas glyphs use `0x2` only.
  uint32_t flags{};
  /// Quad width in layout pixels (for rounded-rect SDF); 0 when unused.
  float rect_w{};
  /// Quad height in layout pixels (for rounded-rect SDF); 0 when unused.
  float rect_h{};
};

}  // namespace eng
