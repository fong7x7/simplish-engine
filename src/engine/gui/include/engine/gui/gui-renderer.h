#pragma once

/// @file gui-renderer.h
/// @brief Batched quad/glyph/line GUI renderer with scissor stack; submits to
/// RHI.
/// @par Threading Main thread only.

#include "draw-command.h"
#include "glyph-info.h"
#include "gui-frame-buffers.h"
#include "gui-rect.h"
#include "gui-vertex.h"
#include "scissor-stack.h"

#include <array>
#include <cstdint>
#include <engine/render/rhi-command-list.h>
#include <engine/render/rhi-device.h>
#include <vector>

namespace eng {

/// `GuiRendererContext::scene_split` when no scene split was marked.
inline constexpr size_t NO_SCENE_SPLIT = static_cast<size_t>(-1);

using RhiTextureHandle = uint64_t;

/// Default vertex buffer capacity (number of vertices).
inline constexpr uint32_t DEFAULT_VERTEX_CAPACITY = 65536;
/// Default index buffer capacity (number of indices).
inline constexpr uint32_t DEFAULT_INDEX_CAPACITY = 98304;

/// How many vertex and index buffer pairs the renderer rotates through.
///
/// A frame's draws read its pair on the GPU while the CPU fills the next
/// frame's, so there must be at least as many pairs as frames a backend keeps
/// in flight. That is two for every backend today — Vulkan's
/// `FRAMES_IN_FLIGHT`, `DX12_FRAMES_IN_FLIGHT`, Metal's default
/// `max_frames_in_flight` — and the third leaves room for one configured a
/// frame deeper, since the RHI does not report the number.
inline constexpr uint32_t GUI_FRAME_BUFFER_COUNT = 3;

/// @thread_safety Main thread only.
class GuiRendererContext {
public:
  /// CPU-side vertex data for the current frame.
  std::vector<GuiVertex> vertices{};
  /// CPU-side index data for the current frame.
  std::vector<uint32_t> indices{};
  /// Draw command list for the current frame.
  std::vector<DrawCommand> commands{};
  /// Clipping scissor rect stack.
  ScissorStack scissor_stack;
  /// Command index where a 3D scene composites; `NO_SCENE_SPLIT` when the
  /// frame marked none.
  size_t scene_split = NO_SCENE_SPLIT;

  /// One vertex and index buffer pair per frame in flight, and one spare.
  std::array<GuiFrameBuffers, GUI_FRAME_BUFFER_COUNT> frame_buffers{};
  /// Index into `frame_buffers` of the pair this frame writes and binds;
  /// each `uploadFrame` moves on to the next.
  uint32_t frame_slot = 0;
  /// GPU pipeline handle for GUI rendering.
  uint64_t pipeline = 0;

  /// RHI device pointer for GPU resource management (not owned).
  RhiDevice* device = nullptr;

  /// Layout / coordinate width (window coordinates; matches input space).
  uint32_t viewport_width = 0;
  /// Layout / coordinate height (window coordinates; matches input space).
  uint32_t viewport_height = 0;
  /// Backbuffer width in pixels; 0 = use viewport_width (same as layout).
  uint32_t surface_width = 0;
  /// Backbuffer height in pixels; 0 = use viewport_height.
  uint32_t surface_height = 0;

  /// Create vertex/index buffers, load GUI shaders, create pipeline.
  /// Pass nullptr for device to skip GPU resource creation (unit-test mode).
  bool init(RhiDevice* device = nullptr);

  /// Release all GPU resources.
  void shutdown();

  /// Reset draw command list, prepare for a new frame.
  void beginFrame();

  /// Parameters for emitting a solid or rounded-rect quad.
  struct EmitQuadParams {
    /// Screen rectangle to fill.
    const Rect& rect;
    /// Packed RGBA color.
    uint32_t color;
    /// Corner radius for rounded rects (0 for sharp).
    float corner_radius;
    /// Border width in pixels (0 for filled).
    float border_width;
  };

  /// Emit a solid or rounded-rect quad.
  void emitQuad(const EmitQuadParams& params);

  /// Parameters for emitting a textured quad.
  struct EmitTexturedQuadParams {
    /// Screen rectangle to draw into.
    const Rect& rect;
    /// UV rectangle within the texture.
    const Rect& uv;
    /// GPU texture handle.
    RhiTextureHandle texture;
    /// Tint color (packed RGBA).
    uint32_t color;
  };

  /// Emit a textured quad (batch breaks when the texture changes).
  void emitTexturedQuad(const EmitTexturedQuadParams& params);

  /// Parameters for emitting an SDF text glyph.
  struct EmitGlyphParams {
    /// Screen X position.
    float x;
    /// Screen Y position.
    float y;
    /// Glyph metrics and atlas info.
    const GlyphInfo& glyph;
    /// Scale factor for the glyph.
    float scale;
    /// Packed RGBA color.
    uint32_t color;
  };

  /// Emit an SDF text glyph.
  void emitGlyph(const EmitGlyphParams& params);

  /// Parameters for emitting a line as a thin rotated quad.
  struct EmitLineParams {
    /// Start X coordinate.
    float x0{};
    /// Start Y coordinate.
    float y0{};
    /// End X coordinate.
    float x1{};
    /// End Y coordinate.
    float y1{};
    /// Packed RGBA color.
    uint32_t color{};
    /// Line width in pixels.
    float width = 1.0f;
  };

  /// Emit a line as a thin rotated quad.
  void emitLine(const EmitLineParams& params);

  /// Mark the current point in the paint order as where a 3D scene is
  /// composited.
  ///
  /// Everything emitted before this draws under the scene; everything after
  /// draws over it. The editor marks it between the ground-plane overlays —
  /// grid, axes, placement footprints — and the interface, so geometry
  /// standing on a tile hides the grid it covers instead of being drawn
  /// over by it.
  void markSceneSplit();

  /// Command index the scene composites at, or the command count when no
  /// split was marked, in which case the whole frame draws over the scene.
  [[nodiscard]] size_t sceneSplit() const;

  /// Upload this frame's geometry into the next buffer pair. Call once per
  /// device frame, after `RhiDevice::beginFrame` and before any submission:
  /// each call claims a pair, and the rotation is only safe at one per frame.
  void uploadFrame();

  /// Bind this frame's buffers and pipeline — the pair the last
  /// `uploadFrame` wrote. Needed once per render pass: bindings do not
  /// survive the end of an encoder.
  void bindFrame(RhiCommandList& cmd_list);

  /// Submit the draw commands in `[first, first + count)`.
  void submitCommandRange(RhiCommandList& cmd_list, size_t first, size_t count);

  /// Push a clipping rect (intersected with current top-of-stack).
  void pushScissor(const Rect& rect);

  /// Pop the top clipping rect.
  void popScissor();

  /// Batch, upload vertex/index data, and submit draw calls to the RHI.
  void endFrame(RhiCommandList& cmd_list);
};

}  // namespace eng
