#pragma once

#include <cstdint>
#include <engine/render/rhi-types.h>

namespace eng {

// --- Glyph info ---

/// @brief Atlas-resident glyph metrics and UV coordinates.
/// @thread_safety Immutable value type.
struct GlyphInfo {
  /// Unicode codepoint this glyph represents.
  uint32_t codepoint = 0;
  /// X position within the atlas texture in pixels.
  uint16_t atlas_x = 0;
  /// Y position within the atlas texture in pixels.
  uint16_t atlas_y = 0;
  /// Width of the glyph region in the atlas in pixels.
  uint16_t atlas_w = 0;
  /// Height of the glyph region in the atlas in pixels.
  uint16_t atlas_h = 0;
  /// Horizontal bearing (offset from pen position to glyph left edge).
  float bearing_x = 0.0f;
  /// Vertical bearing (offset from baseline to glyph top edge).
  float bearing_y = 0.0f;
  /// Horizontal advance width for cursor positioning.
  float advance = 0.0f;
  /// Atlas texels → layout pixels (`1 / supersample` when rasterized HiDPI).
  float atlas_layout_scale = 1.0f;
  /// SDF scaling factor for distance field rendering.
  float sdf_scale = 0.0f;
  /// Index of the atlas texture containing this glyph.
  uint32_t atlas_index = 0;
  /// GPU atlas texture for batched textured quads (0 before first GPU sync).
  RhiTextureHandle atlas_texture = RHI_TEXTURE_INVALID;
  /// Normalized UV bounds in the atlas page (for emitTexturedQuad).
  float uv_u0 = 0.0f;
  /// Normalized V at the top edge of the glyph quad in the atlas.
  float uv_v0 = 0.0f;
  /// Normalized U at the right edge of the glyph quad in the atlas.
  float uv_u1 = 1.0f;
  /// Normalized V at the bottom edge of the glyph quad in the atlas.
  float uv_v1 = 1.0f;
};

}  // namespace eng
