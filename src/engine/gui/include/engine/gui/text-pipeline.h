#pragma once

/// @file text-pipeline.h
/// @brief FreeType + HarfBuzz text: SDF atlas, shaping, line breaks, rich text.
/// @par Threading Main thread only.

#include "font-atlas.h"
#include "font-face.h"
#include "glyph-info.h"
#include "rich-text.h"
#include "shaped-run.h"

#include <cstdint>
#include <engine/render/rhi-device.h>
#include <optional>
#include <string_view>
#include <vector>

namespace eng {

/// Italic style flag for font loading.
enum class FontLoadItalic : uint8_t {
  NORMAL,
  ITALIC,
};

/// Invalid atlas texture sentinel (matches `RHI_TEXTURE_INVALID`).
inline constexpr RhiTextureHandle RHI_TEXTURE_INVALID_TEXT =
    RHI_TEXTURE_INVALID;

/// @thread_safety Main thread only.
class TextPipelineContext {
public:
  /// All allocated font atlas pages.
  std::vector<FontAtlas> atlases{};
  /// All loaded font faces.
  std::vector<FontFace> faces{};
  /// Opaque FreeType library handle.
  void* ft_library = nullptr;
  /// Opaque LRU shape cache handle.
  void* shape_cache = nullptr;
  /// When non-null, atlas pixels are uploaded and glyphs reference GPU
  /// textures.
  RhiDevice* gpu_device = nullptr;

  /// Initialise FreeType and HarfBuzz, create the initial atlas texture.
  bool init();

  /// Free all fonts and atlas textures.
  void shutdown();

  /// Load a font face from a file path. Returns face_id on success.
  std::optional<uint32_t> loadFont(std::string_view path, uint16_t weight,
                                   FontLoadItalic italic);

  /// Shape a UTF-8 string using the specified font face.
  ShapedRun shapeText(uint32_t face_id, std::string_view text);

  /// Ensure a glyph is rasterized in the atlas. Returns atlas info or nullptr.
  const GlyphInfo* ensureGlyph(uint32_t face_id, uint32_t codepoint);

  /// Shape rich text with per-span font selection.
  std::vector<ShapedRun> shapeRichText(const RichText& rich_text);

  /// Get the atlas texture handle for a given atlas index.
  RhiTextureHandle getAtlasTexture(uint32_t atlas_index) const;

  /// Set layout pixel height and optional supersampling (e.g. window pixel
  /// density). Raster height ≈ `layout_height * supersample` (capped); clears
  /// cached glyphs for that face.
  void setFontRasterHeight(uint32_t face_id, uint32_t layout_pixel_height,
                           float supersample = 1.0f);
};

}  // namespace eng
