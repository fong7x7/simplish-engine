#pragma once

#include "glyph-info.h"

#include <cstdint>
#include <string>
#include <unordered_map>

namespace eng {

/// CSS-standard normal font weight.
constexpr uint16_t FONT_WEIGHT_NORMAL = 400;

/// Default glyph rasterization height in pixels.
constexpr uint32_t DEFAULT_RASTER_PIXEL_HEIGHT = 14;

/// @thread_safety Main thread only.
struct FontFace {
  /// Unique face identifier assigned at load time.
  uint32_t face_id = 0;
  /// Font family name.
  std::string family{};
  /// Font weight (100-900; 400 = normal, 700 = bold).
  uint16_t weight = FONT_WEIGHT_NORMAL;
  /// True if this face is italic.
  bool italic = false;
  /// Distance from baseline to top of tallest glyph.
  float ascender = 0.0f;
  /// Distance from baseline to bottom of lowest glyph (negative).
  float descender = 0.0f;
  /// Recommended line height in font units.
  float line_height = 0.0f;
  /// Cached glyph info keyed by Unicode codepoint.
  std::unordered_map<uint32_t, GlyphInfo> glyphs{};
  /// Opaque FreeType face (`FT_Face`); owned until `shutdownTextPipeline`.
  void* ft_face = nullptr;
  /// Pixel height passed to `FT_Set_Pixel_Sizes` for rasterization.
  uint32_t raster_pixel_height = DEFAULT_RASTER_PIXEL_HEIGHT;
  /// Rasterize at `raster_pixel_height` but lay out as if divided by this (≥1).
  float layout_supersample = 1.0f;
};

}  // namespace eng
