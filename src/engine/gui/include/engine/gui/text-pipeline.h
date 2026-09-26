#pragma once

/// @file text-pipeline.h
/// @brief FreeType + HarfBuzz text: SDF atlas, shaping, line breaks, rich text.
/// @par Threading Main thread only.

#include "font-atlas.h"
#include "font-face.h"
#include "font-metrics.h"
#include "glyph-info.h"
#include "rich-text.h"
#include "shaped-run.h"

#include <array>
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

/// The weights `loadFontFamily` loads: regular, medium, semibold, bold —
/// what the theme's text roles ask for.
inline constexpr std::array<uint16_t, 4> GUI_FONT_FAMILY_WEIGHTS{400, 500, 600,
                                                                 700};

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

  /// Load a font from a file path at @p weight (100–900) and slant.
  /// Returns face_id on success. A variable font is set to that weight on
  /// its `wght` axis; a collection (.ttc) opens the face whose OS/2 weight
  /// is nearest; a file with nothing heavy enough is thickened in software
  /// for weights of 600 and up.
  std::optional<uint32_t> loadFont(std::string_view path, uint16_t weight,
                                   FontLoadItalic italic);

  /// Load @p path at every weight in `GUI_FONT_FAMILY_WEIGHTS`, upright,
  /// so text in any role finds a face near its weight. Returns the
  /// regular face's id; nothing if the file does not load.
  std::optional<uint32_t> loadFontFamily(std::string_view path);

  /// Set every loaded face's default size to @p layout_pixel_height at
  /// @p supersample, as `setFontRasterHeight` does for one.
  void setAllFontsRasterHeight(uint32_t layout_pixel_height, float supersample);

  /// Shape a UTF-8 string using the specified font face.
  ShapedRun shapeText(uint32_t face_id, std::string_view text);

  /// Ensure a glyph is rasterized in the atlas at the face's default size.
  /// Returns atlas info or nullptr.
  const GlyphInfo* ensureGlyph(uint32_t face_id, uint32_t codepoint);

  /// Ensure a glyph is rasterized for drawing @p layout_px high (text
  /// size in layout pixels), at the face's supersample so it stays crisp on
  /// a dense display. Each size is its own atlas entry.
  const GlyphInfo* ensureGlyph(uint32_t face_id, uint32_t codepoint,
                               float layout_px);

  /// Ascender, descender and line spacing of @p face_id at @p layout_px,
  /// in layout pixels; zeros for an unknown face.
  [[nodiscard]] FontMetrics metrics(uint32_t face_id, float layout_px) const;

  /// How far to move the pen between @p left and @p right, two glyphs of
  /// one face at one size, beyond the left one's advance: negative for a
  /// pair that tucks in, like "AV". Zero when the font has no kerning.
  [[nodiscard]] float kerning(uint32_t face_id, const GlyphInfo& left,
                              const GlyphInfo& right) const;

  /// The loaded face closest to @p weight with @p italic, preferring the
  /// right slant; nothing when none is loaded.
  [[nodiscard]] std::optional<uint32_t> faceFor(uint16_t weight,
                                                FontLoadItalic italic) const;

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
