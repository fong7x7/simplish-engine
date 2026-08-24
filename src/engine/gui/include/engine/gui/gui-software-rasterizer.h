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
/// Scope limits (what WILL and WON'T be correctly visualised):
///   - Solid-coloured axis-aligned quads (every panel fill) — rendered
///     with alpha compositing. Perfect for layout verification.
///   - Borders / thick lines — approximated as their axis-aligned
///     bounding box. Legible but not pixel-perfect.
///   - Rounded rects — rendered as straight rects at the quad bounding
///     box. Corner radius is ignored; colour is correct.
///   - Text glyphs are emitted as textured quads; this rasterizer
///     paints their fill colour across the bounding box (readable
///     silhouettes, not legible text).
///   - Shader effects (drop shadows, SDF anti-aliasing, gradients)
///     are not rendered; only base fill colours appear.
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
  /// Rasterize `vertices` (groups of 4 per quad, in the layout emitted
  /// by `GuiRendererContext`) into an `ImageData` sized to `viewport`.
  /// The pixel buffer is pre-filled with `background_color` before
  /// each quad is composited (alpha-over blending). Vertices outside
  /// the viewport are clipped.
  static ImageData rasterizeQuads(std::span<const GuiVertex> vertices,
                                  const Rect& viewport,
                                  uint32_t background_color);

  /// Write an RGBA8 `ImageData` to `path` as a PNG. Returns `true` on
  /// success, `false` on invalid input (zero-size image) or I/O
  /// failure (permission denied, disk full, invalid path). Creates
  /// parent directories if they do not yet exist.
  static bool writePng(const ImageData& image, std::string_view path);
};

}  // namespace eng
