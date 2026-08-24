#include <engine/gui/text-pipeline.h>

// NOLINTBEGIN(llvm-include-order) — FreeType requires ft2build.h before
// FT_FREETYPE_H
#include <ft2build.h>
#include FT_FREETYPE_H
// NOLINTEND(llvm-include-order)

#include <algorithm>
#include <cmath>
#include <cstring>
#include <engine/render/rhi-device.h>
#include <engine/render/rhi-texture-desc.h>
#include <engine/render/rhi-texture-update-2d.h>

namespace eng {

/// Initial atlas texture width in pixels.
constexpr uint32_t INITIAL_ATLAS_WIDTH = 1024;
/// Initial atlas texture height in pixels.
constexpr uint32_t INITIAL_ATLAS_HEIGHT = 1024;
/// FreeType 26.6 fixed-point divisor (2^6 = 64).
constexpr float FREETYPE_26_6_SCALE = 64.0f;
/// Number of channels in an RGBA pixel.
constexpr uint32_t RGBA_CHANNELS = 4;

namespace {

  /// Sample one grayscale pixel; `pitch` may be negative (bottom-up rows).
  uint8_t graySample(const FT_Bitmap& bmp, uint32_t row, uint32_t col) {
    if (row >= bmp.rows || col >= static_cast<uint32_t>(bmp.width)) {
      return 0;
    }
    const int pitch_i = static_cast<int>(bmp.pitch);
    const auto abs_p = static_cast<uint32_t>(pitch_i < 0 ? -pitch_i : pitch_i);
    const size_t row_off =
        pitch_i >= 0 ? static_cast<size_t>(row) * abs_p
                     : static_cast<size_t>(bmp.rows - 1u - row) * abs_p;
    return bmp.buffer[row_off + col];
  }

  /// Copy an 8-bit grayscale FreeType bitmap into the atlas as premultiplied
  /// white RGBA.
  void blitGrayToAtlas(FontAtlas& page, uint32_t dst_x, uint32_t dst_y,
                       const FT_Bitmap& bmp) {
    for (uint32_t row = 0; row < bmp.rows; ++row) {
      for (uint32_t col = 0; col < bmp.width; ++col) {
        const auto di =
            ((dst_y + row) * page.width + (dst_x + col)) * RGBA_CHANNELS;
        if (di + 3 >= page.rgba_pixels.size()) {
          return;
        }
        const uint8_t a = graySample(bmp, row, col);
        constexpr uint8_t GLYPH_CHANNEL_MAX = 255;
        page.rgba_pixels[di] = GLYPH_CHANNEL_MAX;
        page.rgba_pixels[di + 1] = GLYPH_CHANNEL_MAX;
        page.rgba_pixels[di + 2] = GLYPH_CHANNEL_MAX;
        page.rgba_pixels[di + 3] = a;
      }
    }
  }

  /// Refresh ascender / descender / line_height from the active FT size.
  void refreshFaceVerticalMetrics(FontFace& face_entry) {
    auto* ft = static_cast<FT_Face>(face_entry.ft_face);
    if (ft == nullptr) {
      return;
    }
    const float inv_ss = 1.0f / std::max(1.0f, face_entry.layout_supersample);
    face_entry.ascender =
        (static_cast<float>(ft->size->metrics.ascender) / FREETYPE_26_6_SCALE) *
        inv_ss;
    face_entry.descender = (static_cast<float>(ft->size->metrics.descender) /
                            FREETYPE_26_6_SCALE) *
                           inv_ss;
    face_entry.line_height =
        (static_cast<float>(ft->size->metrics.height) / FREETYPE_26_6_SCALE) *
        inv_ss;
  }

  /// Point every cached glyph at `tex` after atlas GPU resource is recreated.
  void refreshGlyphAtlasTextureHandles(TextPipelineContext& ctx,
                                       uint32_t atlas_page,
                                       RhiTextureHandle tex) {
    for (auto& face : ctx.faces) {
      for (auto& kv : face.glyphs) {
        GlyphInfo& g = kv.second;
        if (g.atlas_index == atlas_page) {
          g.atlas_texture = tex;
        }
      }
    }
  }

  RhiTextureHandle createAtlasGpuTexture(RhiDevice& dev, FontAtlas& page) {
    RhiTextureDesc desc{};
    desc.width = page.width;
    desc.height = page.height;
    desc.format = RhiFormat::RGB_A8_UNORM;
    desc.initial_pixels = page.rgba_pixels.data();
    return dev.createTexture(desc);
  }

  bool tryUploadAtlasInPlace(RhiDevice& dev, RhiTextureHandle tex,
                             FontAtlas& page) {
    RhiTextureUpdate2D upd{};
    upd.pixels = page.rgba_pixels.data();
    upd.width = page.width;
    upd.height = page.height;
    upd.bytes_per_row = page.width * RGBA_CHANNELS;
    upd.format = RhiFormat::RGB_A8_UNORM;
    return dev.updateTexture2D(tex, upd);
  }

  bool atlasTextureHandleOk(RhiTextureHandle t) {
    return t != RHI_TEXTURE_INVALID;
  }

  /// Replace atlas GPU texture when in-place upload fails.
  void replaceAtlasTexture(TextPipelineContext& ctx, FontAtlas& page) {
    const RhiTextureHandle old_tex = page.texture;
    const RhiTextureHandle new_tex =
        createAtlasGpuTexture(*ctx.gpu_device, page);
    if (!atlasTextureHandleOk(new_tex)) {
      return;
    }
    ctx.gpu_device->destroyTexture(old_tex);
    page.texture = new_tex;
    refreshGlyphAtlasTextureHandles(ctx, 0, page.texture);
  }

  /// Validate atlas pixel buffer has enough data for the texture dimensions.
  bool atlasPixelBufferValid(const FontAtlas& page) {
    const size_t expected = static_cast<size_t>(page.width) *
                            static_cast<size_t>(page.height) * RGBA_CHANNELS;
    return page.rgba_pixels.size() >= expected;
  }

  /// Upload atlas page 0: in-place update when the GPU texture already exists.
  // Algorithm: syncAtlasToGpu — assembly and validation steps.
  void syncAtlasToGpu(TextPipelineContext& ctx, FontAtlas& page) {
    if (ctx.gpu_device == nullptr || page.rgba_pixels.empty()) {
      return;
    }
    if (!atlasPixelBufferValid(page)) {
      return;
    }
    if (!atlasTextureHandleOk(page.texture)) {
      page.texture = createAtlasGpuTexture(*ctx.gpu_device, page);
      if (atlasTextureHandleOk(page.texture)) {
        refreshGlyphAtlasTextureHandles(ctx, 0, page.texture);
      }
      return;
    }
    if (!tryUploadAtlasInPlace(*ctx.gpu_device, page.texture, page)) {
      replaceAtlasTexture(ctx, page);
    }
  }

  /// Maximum font supersample factor.
  constexpr float MAX_SUPERSAMPLE = 3.0f;

  float clampFontSupersample(float supersample) {
    return std::clamp(supersample, 1.0f, MAX_SUPERSAMPLE);
  }

  /// Maximum rasterized pixel height for a glyph.
  constexpr uint32_t MAX_RASTER_PX = 96;

  /// Padding in pixels between atlas glyphs.
  constexpr uint32_t GLYPH_PAD = 1;

  uint32_t rasterPixelsForLayout(uint32_t layout_px, float supersample) {
    const float raw = std::round(static_cast<float>(layout_px) * supersample);
    return static_cast<uint32_t>(std::min<float>(
        static_cast<float>(MAX_RASTER_PX), std::max(1.0f, raw)));
  }

  void populateGlyphMetrics(GlyphInfo& gi, FT_Face ft_face, uint32_t codepoint,
                            float layout_supersample) {
    gi.codepoint = codepoint;
    gi.atlas_w = static_cast<uint16_t>(ft_face->glyph->bitmap.width);
    gi.atlas_h = static_cast<uint16_t>(ft_face->glyph->bitmap.rows);
    const float inv_ss = 1.0f / std::max(1.0f, layout_supersample);
    gi.bearing_x = static_cast<float>(ft_face->glyph->bitmap_left) * inv_ss;
    gi.bearing_y = static_cast<float>(ft_face->glyph->bitmap_top) * inv_ss;
    gi.advance =
        (static_cast<float>(ft_face->glyph->advance.x) / FREETYPE_26_6_SCALE) *
        inv_ss;
    gi.atlas_layout_scale = inv_ss;
    gi.atlas_index = 0;
  }

  /// Advance atlas cursor to a new row if the glyph doesn't fit horizontally.
  void advanceAtlasRowIfNeeded(FontAtlas& page, uint32_t gw) {
    if (page.cursor_x + gw + GLYPH_PAD > page.width) {
      page.cursor_y += page.row_height + GLYPH_PAD;
      page.cursor_x = 0;
      page.row_height = 0;
    }
  }

  // Algorithm: layoutGlyphBitmapInAtlas — assembly and validation steps.
  bool layoutGlyphBitmapInAtlas(FontAtlas& page, GlyphInfo& gi, FT_Face ft) {
    const uint32_t gw = gi.atlas_w;
    const uint32_t gh = gi.atlas_h;
    if (gw == 0 || gh == 0) {
      return true;
    }
    advanceAtlasRowIfNeeded(page, gw);
    if (page.cursor_y + gh > page.height) {
      return false;
    }
    blitGrayToAtlas(page, page.cursor_x, page.cursor_y, ft->glyph->bitmap);
    gi.atlas_x = static_cast<uint16_t>(page.cursor_x);
    gi.atlas_y = static_cast<uint16_t>(page.cursor_y);
    page.row_height = std::max(page.row_height, gh);
    page.cursor_x += gw + GLYPH_PAD;
    return true;
  }

  void setGlyphUvCoords(GlyphInfo& gi, uint32_t gw, uint32_t gh,
                        const FontAtlas& page) {
    const float inv_w = 1.0f / static_cast<float>(page.width);
    const float inv_h = 1.0f / static_cast<float>(page.height);
    gi.uv_u0 = static_cast<float>(gi.atlas_x) * inv_w;
    gi.uv_v0 = static_cast<float>(gi.atlas_y) * inv_h;
    gi.uv_u1 = static_cast<float>(gi.atlas_x + gw) * inv_w;
    gi.uv_v1 = static_cast<float>(gi.atlas_y + gh) * inv_h;
  }

  /// Parameters for uploading a glyph texture.
  struct UploadGlyphParams {
    /// Text pipeline context.
    TextPipelineContext& ctx;
    /// Font atlas page.
    FontAtlas& page;
    /// Glyph info to update.
    GlyphInfo& gi;
    /// Glyph width.
    uint32_t gw;
  };

  void uploadGlyphTexture(const UploadGlyphParams& p, uint32_t gh) {
    auto& ctx = p.ctx;
    auto& page = p.page;
    auto& gi = p.gi;
    uint32_t gw = p.gw;
    if (gw > 0 && gh > 0) {
      syncAtlasToGpu(ctx, page);
      gi.atlas_texture = page.texture;
      return;
    }
    gi.atlas_texture = RHI_TEXTURE_INVALID;
  }

}  // namespace

bool TextPipelineContext::init() {
  FontAtlas atlas;
  atlas.width = INITIAL_ATLAS_WIDTH;
  atlas.height = INITIAL_ATLAS_HEIGHT;
  atlas.rgba_pixels.assign(
      static_cast<size_t>(atlas.width) * atlas.height * RGBA_CHANNELS, 0);
  atlases.push_back(std::move(atlas));

  FT_Library ft = nullptr;
  if (FT_Init_FreeType(&ft) != 0) {
    return false;
  }
  ft_library = ft;
  return true;
}

namespace {

  /// Destroy all atlas GPU textures via the device.
  void destroyAtlasTextures(RhiDevice& dev,
                            std::vector<FontAtlas>& atlas_pages) {
    for (auto& page : atlas_pages) {
      if (page.texture != RHI_TEXTURE_INVALID) {
        dev.destroyTexture(page.texture);
      }
    }
  }

  /// Release all FreeType face handles and null the pointers.
  void releaseFreeTypeFaces(std::vector<FontFace>& face_list) {
    for (auto& face_entry : face_list) {
      if (face_entry.ft_face != nullptr) {
        FT_Done_Face(static_cast<FT_Face>(face_entry.ft_face));
        face_entry.ft_face = nullptr;
      }
    }
  }

}  // namespace

// Algorithm: Structured control flow (data assembly and checks).
void TextPipelineContext::shutdown() {
  if (gpu_device != nullptr) {
    destroyAtlasTextures(*gpu_device, atlases);
  }
  releaseFreeTypeFaces(faces);
  if (ft_library != nullptr) {
    FT_Done_FreeType(static_cast<FT_Library>(ft_library));
  }
  atlases.clear();
  faces.clear();
  ft_library = nullptr;
  shape_cache = nullptr;
  gpu_device = nullptr;
}

/// Default pixel size set on newly loaded font faces.
constexpr uint32_t DEFAULT_PIXEL_SIZE = 14;

/// Build a FontFace entry from a loaded FreeType face.
struct BuildFontFaceParams {
  /// Assigned face identifier.
  uint32_t face_id;
  /// Font weight (100–900).
  uint16_t weight;
  /// Whether the font is italic.
  FontLoadItalic italic;
};

FontFace buildFontFace(FT_Face face, const BuildFontFaceParams& p) {
  FontFace ff;
  ff.face_id = p.face_id;
  ff.family = face->family_name != nullptr ? face->family_name : "";
  ff.weight = p.weight;
  ff.italic = (p.italic == FontLoadItalic::ITALIC);
  ff.ft_face = face;
  ff.raster_pixel_height = DEFAULT_PIXEL_SIZE;
  refreshFaceVerticalMetrics(ff);
  return ff;
}

std::optional<uint32_t> TextPipelineContext::loadFont(
    std::string_view path, uint16_t weight,
    // Algorithm: Structured control flow (data assembly and checks).
    FontLoadItalic italic) {
  if (ft_library == nullptr) {
    return std::nullopt;
  }
  auto* ft = static_cast<FT_Library>(ft_library);

  FT_Face face = nullptr;
  std::string path_str(path);
  if (FT_New_Face(ft, path_str.c_str(), 0, &face) != 0) {
    return std::nullopt;
  }

  FT_Set_Pixel_Sizes(face, 0, DEFAULT_PIXEL_SIZE);
  auto face_id = static_cast<uint32_t>(faces.size() + 1);
  faces.push_back(buildFontFace(face, {face_id, weight, italic}));
  return face_id;
}

ShapedRun TextPipelineContext::shapeText(uint32_t face_id,
                                         std::string_view text) {
  ShapedRun run;
  run.face_id = face_id;
  run.total_advance = 0.0f;
  if (text.empty()) {
    return run;
  }
  return run;
}

/// Apply raster height to a font face and refresh metrics.
void applyRasterHeight(FontFace& face_entry, float ss, uint32_t raster_h) {
  face_entry.layout_supersample = ss;
  face_entry.raster_pixel_height = raster_h;
  if (face_entry.ft_face == nullptr) {
    return;
  }
  auto* ft = static_cast<FT_Face>(face_entry.ft_face);
  if (FT_Set_Pixel_Sizes(ft, 0, raster_h) != 0) {
    return;
  }
  refreshFaceVerticalMetrics(face_entry);
  face_entry.glyphs.clear();
}

void TextPipelineContext::setFontRasterHeight(
    uint32_t face_id, uint32_t layout_pixel_height,
    // Algorithm: Structured control flow (data assembly and checks).
    float supersample) {
  const float ss = clampFontSupersample(supersample);
  const uint32_t raster_h = rasterPixelsForLayout(layout_pixel_height, ss);
  for (auto& face_entry : faces) {
    if (face_entry.face_id == face_id) {
      applyRasterHeight(face_entry, ss, raster_h);
      return;
    }
  }
}

static FT_Face loadGlyphBitmap(FontFace& face, uint32_t codepoint) {
  auto* ft_face = static_cast<FT_Face>(face.ft_face);
  if (ft_face == nullptr) {
    return nullptr;
  }
  FT_Set_Pixel_Sizes(ft_face, 0, face.raster_pixel_height);
  auto glyph_idx = FT_Get_Char_Index(ft_face, codepoint);
  if (FT_Load_Glyph(ft_face, glyph_idx, FT_LOAD_RENDER) != 0) {
    return nullptr;
  }
  return ft_face;
}

/// Rasterize a single glyph, place it in the atlas, and store in the face
/// cache.
const GlyphInfo* rasterizeAndCacheGlyph(TextPipelineContext& ctx,
                                        FontFace& face, FontAtlas& page,
                                        uint32_t codepoint) {
  auto* ft_face = loadGlyphBitmap(face, codepoint);
  if (ft_face == nullptr) {
    return nullptr;
  }
  GlyphInfo gi;
  populateGlyphMetrics(gi, ft_face, codepoint, face.layout_supersample);
  const uint32_t gw = gi.atlas_w;
  const uint32_t gh = gi.atlas_h;
  if (!layoutGlyphBitmapInAtlas(page, gi, ft_face)) {
    return nullptr;
  }
  setGlyphUvCoords(gi, gw, gh, page);
  uploadGlyphTexture({ctx, page, gi, gw}, gh);
  auto [inserted, _] = face.glyphs.emplace(codepoint, gi);
  return &inserted->second;
}

/// Find a font face by ID, or return nullptr.
FontFace* findFaceById(std::vector<FontFace>& face_list, uint32_t face_id) {
  for (auto& face_entry : face_list) {
    if (face_entry.face_id == face_id) {
      return &face_entry;
    }
  }
  return nullptr;
}

const GlyphInfo* TextPipelineContext::ensureGlyph(
    uint32_t face_id,
    // Algorithm: Structured control flow (data assembly and checks).
    uint32_t codepoint) {
  if (atlases.empty()) {
    return nullptr;
  }
  auto* face = findFaceById(faces, face_id);
  if (face == nullptr) {
    return nullptr;
  }
  auto it = face->glyphs.find(codepoint);
  if (it != face->glyphs.end()) {
    return &it->second;
  }
  return rasterizeAndCacheGlyph(*this, *face, atlases[0], codepoint);
}

std::vector<ShapedRun>
TextPipelineContext::shapeRichText(const RichText& rich_text) {
  if (rich_text.spans.empty()) {
    return {};
  }

  std::vector<ShapedRun> runs;
  runs.reserve(rich_text.spans.size());
  for (const auto& span : rich_text.spans) {
    auto slice = rich_text.text.substr(span.start, span.end - span.start);
    runs.push_back(shapeText(0, slice));
  }
  return runs;
}

RhiTextureHandle
TextPipelineContext::getAtlasTexture(uint32_t atlas_index) const {
  if (atlas_index >= atlases.size()) {
    return RHI_TEXTURE_INVALID;
  }
  return atlases[atlas_index].texture;
}

}  // namespace eng
