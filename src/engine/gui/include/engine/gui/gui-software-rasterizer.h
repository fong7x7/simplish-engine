#pragma once

/// @file gui-software-rasterizer.h
/// @brief CPU-only rasterizer that converts a `GuiRendererContext` vertex
/// buffer into an RGBA8 pixel image, independent of any RHI backend.
/// Designed for platform-agnostic screenshot generation and headless
/// visual-feedback loops.
///
/// Primary consumers:
///   - Test support (`tests/engine/gui/support/gui_image_capture.h`)
///     drives a `GuiTestHarness`, renders, and calls this rasterizer.
///   - Editor "save screenshot" command (future) captures the editor's
///     live vertex buffer to PNG without going through the GPU.
///   - Game debug captures / error reports.
///
/// What it draws: every shape quad — fills, rounded and per-corner
/// radii, per-side borders, linear and radial gradients, soft shadows —
/// shaded per pixel by the same maths as the GPU shader
/// (`gui-quad-shading.h`), so a capture shows corners, rings and shadows
/// as the editor does. Lines are filled across their bounding box. Glyph
/// quads are sampled from the font atlas when given one
/// (`rasterizeQuads` taking a `GlyphAtlas`), else painted as boxes.
/// Colours blend in sRGB bytes rather than linear light, so
/// translucent overlaps and gradient midpoints differ a little from the
/// GPU's, and scissor clips are not applied.
///
/// For pixel-perfect rendering, capture from the real RHI backend and
/// GPU-readback into `ImageData`. The interface below remains identical
/// either way.
/// @par Threading Main thread only (performs disk I/O on `writePng`).

#include <cstdint>
#include <engine/gui/gui-rect.h>
#include <engine/gui/gui-vertex.h>
#include <engine/gui/image-data.h>
#include <span>
#include <string_view>

namespace eng {

/// Default background for rasterized captures — matches the dark editor
/// theme panel token so empty regions don't appear black. Packed with
/// `GuiColor::pack()` layout (R low byte, A high byte); as a hex
/// literal this reads `0xAABBGGRR`, so `0xFF1E1E1E` = opaque dark grey.
inline constexpr uint32_t GUI_RASTER_DEFAULT_BG = 0xFF1E1E1EU;

/// Stateless software rasterizer + PNG writer. All methods are `static`
/// and thread-safe against distinct inputs; they do not share state.
/// @thread_safety Callable from the main thread; no shared global state.
struct GuiSoftwareRasterizer {
  /// Read-only view of a CPU-side font atlas page
  /// (`TextPipelineContext::atlases[n]`), used to sample glyph quads.
  ///
  /// Pixels are RGBA8 with coverage in the alpha channel, which is the
  /// layout the text pipeline blits glyph bitmaps into.
  struct GlyphAtlas {
    /// Atlas pixels, row-major RGBA8. Empty disables glyph sampling.
    std::span<const uint8_t> rgba_pixels{};
    /// Atlas width in pixels.
    uint32_t width = 0;
    /// Atlas height in pixels.
    uint32_t height = 0;
  };

  /// Rasterize `vertices` (groups of 4 per quad, in the layout emitted
  /// by `GuiRendererContext`) into an `ImageData` sized to `viewport`.
  /// The pixel buffer is pre-filled with `background_color` before
  /// each quad is composited (alpha-over blending). Vertices outside
  /// the viewport are clipped. Textured quads paint as solid boxes;
  /// pass a `GlyphAtlas` to sample them instead.
  static ImageData rasterizeQuads(std::span<const GuiVertex> vertices,
                                  const Rect& viewport,
                                  uint32_t background_color);

  /// As above, sampling textured quads from `atlas` so text is legible.
  ///
  /// Every textured quad is assumed to come from this atlas — the vertex
  /// format carries UVs but not which texture they index, so a capture
  /// mixing glyphs with image quads would sample both from here.
  static ImageData rasterizeQuads(std::span<const GuiVertex> vertices,
                                  const Rect& viewport,
                                  uint32_t background_color,
                                  const GlyphAtlas& atlas);

  /// Write an RGBA8 `ImageData` to `path` as a PNG. Returns `true` on
  /// success, `false` on invalid input (zero-size image) or I/O
  /// failure (permission denied, disk full, invalid path). Creates
  /// parent directories if they do not yet exist.
  static bool writePng(const ImageData& image, std::string_view path);
};

}  // namespace eng
