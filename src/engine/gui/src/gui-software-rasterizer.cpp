/// @file gui-software-rasterizer.cpp
/// @brief Implementation of the CPU rasterizer and PNG writer. See
/// header for scope and limitations.

#include "engine/gui/gui-software-rasterizer.h"

// Use stb_image_write declarations only. The single-file implementation
// is already compiled into each RHI backend (dx12 / metal / vulkan /
// opengl) via its own `stb-image-write-impl.cpp` — re-defining
// `STB_IMAGE_WRITE_IMPLEMENTATION` here would cause duplicate symbols
// when linking a binary that picks one of those backends (which all
// editor/game/test binaries do).
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wold-style-cast"
#pragma clang diagnostic ignored "-Wsign-conversion"
#pragma clang diagnostic ignored "-Wcast-qual"
#include <stb_image_write.h>
#pragma clang diagnostic pop

#include <algorithm>
#include <array>
#include <cstddef>
#include <filesystem>
#include <string>

namespace eng {

namespace {

  /// Number of bytes per pixel in RGBA8 output.
  constexpr uint32_t RGBA_BYTES = 4;

  /// Number of vertices per UI quad (see `appendStyledQuadVertices` in
  /// `engine/gui/src/gui-renderer.cpp`).
  constexpr size_t VERTICES_PER_QUAD = 4;

  /// `GuiVertex::flags` bit marking a textured quad; glyphs use it alone.
  constexpr uint32_t VERTEX_FLAG_TEXTURED = 0x2U;

  /// Smallest quad extent used as a divisor, to keep the UV map finite.
  constexpr float MIN_QUAD_EXTENT = 1e-4F;

  /// Fractional conversion factor for RGBA byte components.
  constexpr float BYTE_TO_UNIT = 1.0F / 255.0F;
  /// Scale factor when packing a unit float back to a byte.
  constexpr float UNIT_TO_BYTE = 255.0F;

  /// Pixel colour broken into unit floats for compositing. Source
  /// colours are packed in `GuiColor::pack()` order: R at low byte, G
  /// at byte 1, B at byte 2, A at high byte. When written as a hex
  /// literal this reads as `0xAABBGGRR`.
  struct FloatRgba {
    /// Red component in [0, 1].
    float r = 0.0F;
    /// Green component in [0, 1].
    float g = 0.0F;
    /// Blue component in [0, 1].
    float b = 0.0F;
    /// Alpha component in [0, 1].
    float a = 0.0F;
  };

  /// Axis-aligned pixel bounds [x_min, x_max) × [y_min, y_max), clipped
  /// to the image.
  struct PixelBounds {
    /// Left pixel column (inclusive).
    int32_t x_min = 0;
    /// Right pixel column (exclusive).
    int32_t x_max = 0;
    /// Top pixel row (inclusive).
    int32_t y_min = 0;
    /// Bottom pixel row (exclusive).
    int32_t y_max = 0;
  };

  /// Unpack a packed RGBA8 colour (`GuiColor::pack()` layout: R low
  /// byte, G byte 1, B byte 2, A high byte) into unit floats.
  FloatRgba unpackRgba(uint32_t packed) {
    FloatRgba c;
    c.r = static_cast<float>(packed & 0xFFU) * BYTE_TO_UNIT;
    c.g = static_cast<float>((packed >> 8U) & 0xFFU) * BYTE_TO_UNIT;
    c.b = static_cast<float>((packed >> 16U) & 0xFFU) * BYTE_TO_UNIT;
    c.a = static_cast<float>((packed >> 24U) & 0xFFU) * BYTE_TO_UNIT;
    return c;
  }

  /// Clamp a float to [0, 1] then convert to a byte.
  uint8_t toByte(float v) {
    return static_cast<uint8_t>(std::clamp(v, 0.0F, 1.0F) * UNIT_TO_BYTE);
  }

  /// Unclipped axis-aligned bounding box of a 4-vertex quad, in float
  /// layout-pixel coordinates.
  struct FloatBounds {
    /// Minimum x across all 4 corners.
    float min_x = 0.0F;
    /// Maximum x across all 4 corners.
    float max_x = 0.0F;
    /// Minimum y across all 4 corners.
    float min_y = 0.0F;
    /// Maximum y across all 4 corners.
    float max_y = 0.0F;
  };

  /// Compute the float bounding box of a 4-vertex quad.
  FloatBounds
  quadFloatBounds(const std::array<GuiVertex, VERTICES_PER_QUAD>& quad) {
    FloatBounds b{quad[0].pos[0], quad[0].pos[0], quad[0].pos[1],
                  quad[0].pos[1]};
    for (size_t i = 1; i < VERTICES_PER_QUAD; ++i) {
      b.min_x = std::min(b.min_x, quad[i].pos[0]);
      b.min_y = std::min(b.min_y, quad[i].pos[1]);
      b.max_x = std::max(b.max_x, quad[i].pos[0]);
      b.max_y = std::max(b.max_y, quad[i].pos[1]);
    }
    return b;
  }

  /// Clamp a float bbox to the image dimensions, producing an integer
  /// pixel-rect bounds (half-open).
  PixelBounds clampToPixels(const FloatBounds& f, uint32_t image_w,
                            uint32_t image_h) {
    PixelBounds b;
    auto w_int = static_cast<int32_t>(image_w);
    auto h_int = static_cast<int32_t>(image_h);
    b.x_min = std::clamp(static_cast<int32_t>(f.min_x), 0, w_int);
    b.x_max = std::clamp(static_cast<int32_t>(f.max_x), 0, w_int);
    b.y_min = std::clamp(static_cast<int32_t>(f.min_y), 0, h_int);
    b.y_max = std::clamp(static_cast<int32_t>(f.max_y), 0, h_int);
    return b;
  }

  /// Compute the axis-aligned bounding box of a 4-vertex quad, clipped
  /// to the image dimensions.
  PixelBounds
  quadBoundsClipped(const std::array<GuiVertex, VERTICES_PER_QUAD>& quad,
                    uint32_t image_w, uint32_t image_h) {
    return clampToPixels(quadFloatBounds(quad), image_w, image_h);
  }

  /// Alpha-over composite: `dst = src.a * src + (1 - src.a) * dst`.
  FloatRgba compositeOver(const FloatRgba& src, const FloatRgba& dst) {
    FloatRgba out;
    float one_minus_a = 1.0F - src.a;
    out.r = src.a * src.r + one_minus_a * dst.r;
    out.g = src.a * src.g + one_minus_a * dst.g;
    out.b = src.a * src.b + one_minus_a * dst.b;
    out.a = src.a + one_minus_a * dst.a;
    return out;
  }

  /// Mutable view over the target pixel buffer — groups the buffer
  /// pointer and stride (image width) into one argument so pixel
  /// helpers stay within the 4-parameter function limit.
  struct PixelTarget {
    /// Pointer to the RGBA8 pixel buffer (row-major).
    std::vector<uint8_t>* pixels = nullptr;
    /// Image width in pixels (stride = width × RGBA_BYTES).
    uint32_t width = 0;
    /// Font atlas used to sample glyph quads; empty paints them solid.
    GuiSoftwareRasterizer::GlyphAtlas glyphs{};

    /// Whether glyph quads can be sampled rather than painted solid.
    [[nodiscard]] bool hasGlyphAtlas() const {
      return !glyphs.rgba_pixels.empty();
    }
  };

  /// Write an RGBA float colour into the pixel buffer at (x, y).
  void writePixel(const PixelTarget& tgt, int32_t x, int32_t y,
                  const FloatRgba& c) {
    size_t offset =
        (static_cast<size_t>(y) * tgt.width + static_cast<size_t>(x)) *
        RGBA_BYTES;
    (*tgt.pixels)[offset + 0] = toByte(c.r);
    (*tgt.pixels)[offset + 1] = toByte(c.g);
    (*tgt.pixels)[offset + 2] = toByte(c.b);
    (*tgt.pixels)[offset + 3] = toByte(c.a);
  }

  /// Read the pixel buffer at (x, y) as a FloatRgba.
  FloatRgba readPixel(const PixelTarget& tgt, int32_t x, int32_t y) {
    size_t offset =
        (static_cast<size_t>(y) * tgt.width + static_cast<size_t>(x)) *
        RGBA_BYTES;
    FloatRgba c;
    c.r = static_cast<float>((*tgt.pixels)[offset + 0]) * BYTE_TO_UNIT;
    c.g = static_cast<float>((*tgt.pixels)[offset + 1]) * BYTE_TO_UNIT;
    c.b = static_cast<float>((*tgt.pixels)[offset + 2]) * BYTE_TO_UNIT;
    c.a = static_cast<float>((*tgt.pixels)[offset + 3]) * BYTE_TO_UNIT;
    return c;
  }

  /// True when `(x, y)` lies within `stroke_px` of any edge of the half-
  /// open rectangle `[bounds.x_min, bounds.x_max) × [bounds.y_min,
  /// bounds.y_max)`. Used to rasterize border-only quads as an outline
  /// frame instead of a filled box.
  bool isOnBorder(int32_t x, int32_t y, const PixelBounds& bounds,
                  int32_t stroke_px) {
    bool near_left = (x - bounds.x_min) < stroke_px;
    bool near_right = (bounds.x_max - 1 - x) < stroke_px;
    bool near_top = (y - bounds.y_min) < stroke_px;
    bool near_bottom = (bounds.y_max - 1 - y) < stroke_px;
    return near_left || near_right || near_top || near_bottom;
  }

  /// Texture-coordinate extent of a quad, taken as min/max across its
  /// corners so the mapping does not depend on vertex winding.
  struct UvBounds {
    /// Minimum u across all 4 corners.
    float min_u = 0.0F;
    /// Maximum u across all 4 corners.
    float max_u = 0.0F;
    /// Minimum v across all 4 corners.
    float min_v = 0.0F;
    /// Maximum v across all 4 corners.
    float max_v = 0.0F;
  };

  /// Compute the uv bounding box of a 4-vertex quad.
  UvBounds quadUvBounds(const std::array<GuiVertex, VERTICES_PER_QUAD>& quad) {
    UvBounds b{quad[0].uv[0], quad[0].uv[0], quad[0].uv[1], quad[0].uv[1]};
    for (size_t i = 1; i < VERTICES_PER_QUAD; ++i) {
      b.min_u = std::min(b.min_u, quad[i].uv[0]);
      b.max_u = std::max(b.max_u, quad[i].uv[0]);
      b.min_v = std::min(b.min_v, quad[i].uv[1]);
      b.max_v = std::max(b.max_v, quad[i].uv[1]);
    }
    return b;
  }

  /// Affine map from pixel centre to texture coordinate:
  /// `u = u0 + x * du`, `v = v0 + y * dv`.
  struct UvMap {
    /// u at x = 0.
    float u0 = 0.0F;
    /// u change per pixel column.
    float du = 0.0F;
    /// v at y = 0.
    float v0 = 0.0F;
    /// v change per pixel row.
    float dv = 0.0F;
  };

  UvMap makeUvMap(const FloatBounds& box, const UvBounds& uv) {
    const float w = std::max(MIN_QUAD_EXTENT, box.max_x - box.min_x);
    const float h = std::max(MIN_QUAD_EXTENT, box.max_y - box.min_y);
    UvMap map;
    map.du = (uv.max_u - uv.min_u) / w;
    map.dv = (uv.max_v - uv.min_v) / h;
    map.u0 = uv.min_u + (0.5F - box.min_x) * map.du;
    map.v0 = uv.min_v + (0.5F - box.min_y) * map.dv;
    return map;
  }

  /// Texel index for a normalized coordinate, clamped to [0, extent).
  int32_t texelIndex(float coord, uint32_t extent) {
    const auto scaled =
        static_cast<int32_t>(coord * static_cast<float>(extent));
    return std::clamp(scaled, 0, static_cast<int32_t>(extent) - 1);
  }

  /// Nearest-neighbour sample of the atlas alpha channel, which is where
  /// the text pipeline stores glyph coverage.
  float sampleAtlasAlpha(const GuiSoftwareRasterizer::GlyphAtlas& atlas,
                         float u, float v) {
    if (atlas.width == 0 || atlas.height == 0) {
      return 0.0F;
    }
    const auto ix = static_cast<size_t>(texelIndex(u, atlas.width));
    const auto iy = static_cast<size_t>(texelIndex(v, atlas.height));
    const size_t offset = (iy * atlas.width + ix) * RGBA_BYTES + 3;
    if (offset >= atlas.rgba_pixels.size()) {
      return 0.0F;
    }
    return static_cast<float>(atlas.rgba_pixels[offset]) * BYTE_TO_UNIT;
  }

  /// Composite one glyph quad, modulating its alpha by atlas coverage.
  void compositeGlyphQuad(const PixelTarget& tgt, const PixelBounds& bounds,
                          const UvMap& map, const FloatRgba& src) {
    for (int32_t y = bounds.y_min; y < bounds.y_max; ++y) {
      const float v = map.v0 + static_cast<float>(y) * map.dv;
      for (int32_t x = bounds.x_min; x < bounds.x_max; ++x) {
        const float u = map.u0 + static_cast<float>(x) * map.du;
        FloatRgba px = src;
        px.a = src.a * sampleAtlasAlpha(tgt.glyphs, u, v);
        if (px.a <= 0.0F) {
          continue;
        }
        writePixel(tgt, x, y, compositeOver(px, readPixel(tgt, x, y)));
      }
    }
  }

  /// Composite one solid quad: a filled box, or a frame when the quad
  /// carries a border width.
  void compositeSolidQuad(const PixelTarget& tgt, const PixelBounds& bounds,
                          const FloatRgba& src, int32_t stroke_px) {
    const bool is_border = stroke_px > 0;
    for (int32_t y = bounds.y_min; y < bounds.y_max; ++y) {
      for (int32_t x = bounds.x_min; x < bounds.x_max; ++x) {
        if (is_border && !isOnBorder(x, y, bounds, stroke_px)) {
          continue;
        }
        writePixel(tgt, x, y, compositeOver(src, readPixel(tgt, x, y)));
      }
    }
  }

  /// Composite one quad's bounding box into the pixel buffer.
  void compositeQuad(const PixelTarget& tgt, uint32_t image_h,
                     const std::array<GuiVertex, VERTICES_PER_QUAD>& quad) {
    auto src = unpackRgba(quad[0].color);
    if (src.a <= 0.0F) {
      return;
    }
    auto bounds = quadBoundsClipped(quad, tgt.width, image_h);
    if ((quad[0].flags & VERTEX_FLAG_TEXTURED) != 0 && tgt.hasGlyphAtlas()) {
      const FloatBounds box = quadFloatBounds(quad);
      compositeGlyphQuad(tgt, bounds, makeUvMap(box, quadUvBounds(quad)), src);
      return;
    }
    compositeSolidQuad(
        tgt, bounds, src,
        static_cast<int32_t>(std::max(0.0F, quad[0].border_width)));
  }

  /// Initialise the pixel buffer with the background colour.
  void fillBackground(std::vector<uint8_t>& pixels, FloatRgba bg) {
    size_t pixel_count = pixels.size() / RGBA_BYTES;
    for (size_t i = 0; i < pixel_count; ++i) {
      pixels[i * RGBA_BYTES + 0] = toByte(bg.r);
      pixels[i * RGBA_BYTES + 1] = toByte(bg.g);
      pixels[i * RGBA_BYTES + 2] = toByte(bg.b);
      pixels[i * RGBA_BYTES + 3] = toByte(bg.a);
    }
  }

  /// Iterate quads in `vertices` and composite each onto the pixel buffer.
  void rasterizeAllQuads(std::span<const GuiVertex> vertices, ImageData& image,
                         const GuiSoftwareRasterizer::GlyphAtlas& atlas) {
    PixelTarget tgt{&image.pixels, image.width, atlas};
    size_t quad_count = vertices.size() / VERTICES_PER_QUAD;
    for (size_t q = 0; q < quad_count; ++q) {
      std::array<GuiVertex, VERTICES_PER_QUAD> quad{};
      for (size_t i = 0; i < VERTICES_PER_QUAD; ++i) {
        quad[i] = vertices[q * VERTICES_PER_QUAD + i];
      }
      compositeQuad(tgt, image.height, quad);
    }
  }

  /// Ensure parent directories of `path` exist before writing.
  void ensureParentDir(const std::filesystem::path& fs_path) {
    if (fs_path.has_parent_path()) {
      std::error_code ec;
      std::filesystem::create_directories(fs_path.parent_path(), ec);
    }
  }

}  // namespace

ImageData
GuiSoftwareRasterizer::rasterizeQuads(std::span<const GuiVertex> vertices,
                                      const Rect& viewport,
                                      uint32_t background_color) {
  return rasterizeQuads(vertices, viewport, background_color, GlyphAtlas{});
}

ImageData GuiSoftwareRasterizer::rasterizeQuads(
    std::span<const GuiVertex> vertices, const Rect& viewport,
    uint32_t background_color, const GlyphAtlas& atlas) {
  ImageData image;
  image.width = static_cast<uint32_t>(std::max(0.0F, viewport.w));
  image.height = static_cast<uint32_t>(std::max(0.0F, viewport.h));
  image.source_channels = RGBA_BYTES;
  image.pixels.assign(
      static_cast<size_t>(image.width) * image.height * RGBA_BYTES, 0);
  if (image.width == 0 || image.height == 0) {
    return image;
  }
  fillBackground(image.pixels, unpackRgba(background_color));
  rasterizeAllQuads(vertices, image, atlas);
  return image;
}

bool GuiSoftwareRasterizer::writePng(const ImageData& image,
                                     std::string_view path) {
  if (image.width == 0 || image.height == 0 || image.pixels.empty()) {
    return false;
  }
  std::string path_str(path);
  std::filesystem::path fs_path(path_str);
  ensureParentDir(fs_path);
  int stride_bytes = static_cast<int>(image.width * RGBA_BYTES);
  int result = stbi_write_png(path_str.c_str(), static_cast<int>(image.width),
                              static_cast<int>(image.height), RGBA_BYTES,
                              image.pixels.data(), stride_bytes);
  return result != 0;
}

}  // namespace eng
