#pragma once

/// @file gui-renderer.h
/// @brief Batched quad/glyph/line GUI renderer with scissor stack; submits to
/// RHI.
/// @par Threading Main thread only.

#include "draw-command.h"
#include "glyph-info.h"
#include "gui-rect.h"
#include "gui-vertex.h"
#include "scissor-stack.h"

#include <cstdint>
#include <engine/render/rhi-command-list.h>
#include <engine/render/rhi-device.h>
#include <vector>

namespace eng {

using RhiTextureHandle = uint64_t;

/// Default vertex buffer capacity (number of vertices).
inline constexpr uint32_t DEFAULT_VERTEX_CAPACITY = 65536;
/// Default index buffer capacity (number of indices).
inline constexpr uint32_t DEFAULT_INDEX_CAPACITY = 98304;

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

  /// GPU vertex buffer handle.
  uint64_t vertex_buffer = 0;
  /// GPU index buffer handle.
  uint64_t index_buffer = 0;
  /// GPU pipeline handle for GUI rendering.
  uint64_t pipeline = 0;

  /// Current vertex buffer capacity in vertices.
  uint32_t vertex_capacity = 0;
  /// Current index buffer capacity in indices.
  uint32_t index_capacity = 0;

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

  /// Push a clipping rect (intersected with current top-of-stack).
  void pushScissor(const Rect& rect);

  /// Pop the top clipping rect.
  void popScissor();

  /// Batch, upload vertex/index data, and submit draw calls to the RHI.
  void endFrame(RhiCommandList& cmd_list);
};

}  // namespace eng
