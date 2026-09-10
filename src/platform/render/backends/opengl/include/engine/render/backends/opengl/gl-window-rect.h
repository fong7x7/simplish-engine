#pragma once

#ifdef ENGINE_RENDERER_OPENGL

/// @file gl-window-rect.h
/// @brief A rectangle as glScissor and glViewport take it: from the bottom.
/// @par Threading Thread-safe (value type and pure functions).
///
/// Every RHI caller gives scissors and viewports from the top-left corner
/// of the target, rows counting down, because that is what Metal, DX12 and
/// Vulkan take. GL counts rows up from the bottom of the framebuffer. Passed
/// straight through, a scissor a few rows from the top clips a few rows from
/// the bottom instead — which is how the editor's viewport and every GUI
/// clip rect came out on this backend until these existed.

#include <cstdint>
#include <engine/render/rhi-scissor.h>
#include <engine/render/rhi-viewport.h>

namespace eng::render {

/// A rectangle in GL window coordinates, whose rows count up from the
/// framebuffer's bottom edge.
/// @thread_safety Immutable value type.
struct GlWindowRect {
  /// Left edge in pixels.
  int x = 0;
  /// Bottom edge in pixels, counted up from the framebuffer's bottom row.
  int y = 0;
  /// Width in pixels.
  int width = 0;
  /// Height in pixels.
  int height = 0;
};

/// @p scissor, given from the top-left, as glScissor takes it on a
/// framebuffer @p surface_height pixels tall.
[[nodiscard]] constexpr GlWindowRect glScissorRect(const RhiScissor& scissor,
                                                   uint32_t surface_height) {
  const auto height = static_cast<int>(scissor.height);
  return {scissor.x, static_cast<int>(surface_height) - scissor.y - height,
          static_cast<int>(scissor.width), height};
}

/// @p viewport, given from the top-left, as glViewport takes it on a
/// framebuffer @p surface_height pixels tall.
[[nodiscard]] constexpr GlWindowRect glViewportRect(const RhiViewport& viewport,
                                                    uint32_t surface_height) {
  const float bottom =
      static_cast<float>(surface_height) - viewport.y - viewport.height;
  return {static_cast<int>(viewport.x), static_cast<int>(bottom),
          static_cast<int>(viewport.width), static_cast<int>(viewport.height)};
}

}  // namespace eng::render

#endif  // ENGINE_RENDERER_OPENGL
