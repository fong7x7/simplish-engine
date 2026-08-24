#include <algorithm>
#include <cmath>
#include <cstring>
#include <engine/gui/glyph-info.h>
#include <engine/gui/gui-rect.h>
#include <engine/gui/gui-renderer.h>
#include <engine/gui/gui-vertex-layout.h>
#include <engine/render/rhi-buffer-desc.h>
#include <engine/render/rhi-command-list.h>
#include <engine/render/rhi-device.h>
#include <engine/render/rhi-graphics-pipeline-desc.h>
#include <engine/render/rhi-types.h>

namespace eng {

/// Vertex flag for textured quad.
constexpr uint32_t FLAG_TEXTURED = 0x2;
/// Vertex flag for rounded rectangle.
constexpr uint32_t FLAG_ROUNDED_RECT = 0x4;
/// Minimum line length below which emitLine is a no-op.
constexpr float MIN_LINE_LENGTH = 0.001f;

namespace {

  uint32_t effectiveSurfaceW(const GuiRendererContext& ctx) {
    return ctx.surface_width != 0U ? ctx.surface_width : ctx.viewport_width;
  }

  uint32_t effectiveSurfaceH(const GuiRendererContext& ctx) {
    return ctx.surface_height != 0U ? ctx.surface_height : ctx.viewport_height;
  }

  /// Coordinate/surface dimensions for scissor mapping.
  struct ScissorMapDims {
    /// Layout coordinate width.
    uint32_t coord_w;
    /// Layout coordinate height.
    uint32_t coord_h;
    /// Surface pixel width.
    uint32_t surf_w;
    /// Surface pixel height.
    uint32_t surf_h;
  };

  ScissorCmd mapScissorToSurface(const ScissorCmd& s, const ScissorMapDims& d) {
    uint32_t coord_w = d.coord_w;
    uint32_t coord_h = d.coord_h;
    uint32_t surf_w = d.surf_w;
    uint32_t surf_h = d.surf_h;
    if (coord_w == 0U || coord_h == 0U) {
      return s;
    }
    const float sx = static_cast<float>(surf_w) / static_cast<float>(coord_w);
    const float sy = static_cast<float>(surf_h) / static_cast<float>(coord_h);
    ScissorCmd out{};
    out.x = std::floor(s.x * sx);
    out.y = std::floor(s.y * sy);
    out.w = std::max(1.0f, std::ceil(s.w * sx));
    out.h = std::max(1.0f, std::ceil(s.h * sy));
    return out;
  }

  /// Compute glyph screen-space rect from glyph metrics and scale.
  struct GlyphRect {
    /// Glyph origin X.
    float x = 0;
    /// Glyph origin Y.
    float y = 0;
    /// Glyph width.
    float w = 0;
    /// Glyph height.
    float h = 0;
  };

  GlyphRect computeGlyphRect(float x, float y, const GlyphInfo& glyph,
                             float scale) {
    const float al =
        glyph.atlas_layout_scale > 0.0f ? glyph.atlas_layout_scale : 1.0f;
    return {x + glyph.bearing_x * scale, y - glyph.bearing_y * scale,
            static_cast<float>(glyph.atlas_w) * al * scale,
            static_cast<float>(glyph.atlas_h) * al * scale};
  }

  /// Screen quad with style fields for solid / rounded-rect vertices.
  struct StyledQuadVerts {
    /// Left X coordinate.
    float x0;
    /// Top Y coordinate.
    float y0;
    /// Right X coordinate.
    float x1;
    /// Bottom Y coordinate.
    float y1;
    /// Packed RGBA color.
    uint32_t color;
    /// Corner radius for rounded rects (0 for sharp).
    float corner_radius;
    /// Border width in pixels (0 for filled).
    float border_width;
    /// Vertex shader flags (textured, rounded, etc.).
    uint32_t flags;
  };

  /// Clip-space corners and UVs for one textured quad.
  struct TexturedQuadVerts {
    /// Left X coordinate.
    float x0;
    /// Top Y coordinate.
    float y0;
    /// Right X coordinate.
    float x1;
    /// Bottom Y coordinate.
    float y1;
    /// Left U texture coordinate.
    float u0;
    /// Top V texture coordinate.
    float v0;
    /// Right U texture coordinate.
    float u1;
    /// Bottom V texture coordinate.
    float v1;
    /// Packed RGBA color.
    uint32_t color;
  };

  /// Endpoints and normal half-width for a thick line quad.
  struct ThickLineVerts {
    /// Start X coordinate.
    float x0;
    /// Start Y coordinate.
    float y0;
    /// End X coordinate.
    float x1;
    /// End Y coordinate.
    float y1;
    /// Normal X (perpendicular half-width offset).
    float nx;
    /// Normal Y (perpendicular half-width offset).
    float ny;
    /// Packed RGBA color.
    uint32_t color;
  };

  /// Position and UV for one corner of a styled quad.
  struct CornerPosUV {
    /// Screen-space X.
    float x;
    /// Screen-space Y.
    float y;
    /// Texture U coordinate.
    float u;
    /// Texture V coordinate.
    float v;
  };

  /// Build one styled quad vertex at a given corner position and UV.
  GuiVertex makeStyledVertex(const StyledQuadVerts& q, const CornerPosUV& c) {
    return GuiVertex{{c.x, c.y},     {c.u, c.v}, q.color,     q.corner_radius,
                     q.border_width, q.flags,    q.x1 - q.x0, q.y1 - q.y0};
  }

  // Algorithm: Four GuiVertex corners for one styled rounded quad.
  void appendStyledQuadVertices(GuiRendererContext& ctx,
                                const StyledQuadVerts& q) {
    ctx.vertices.push_back(makeStyledVertex(q, {q.x0, q.y0, 0, 0}));
    ctx.vertices.push_back(makeStyledVertex(q, {q.x1, q.y0, 1, 0}));
    ctx.vertices.push_back(makeStyledVertex(q, {q.x1, q.y1, 1, 1}));
    ctx.vertices.push_back(makeStyledVertex(q, {q.x0, q.y1, 0, 1}));
  }

  void appendTexturedQuadCorners(GuiRendererContext& ctx,
                                 const TexturedQuadVerts& t) {
    ctx.vertices.push_back(GuiVertex{
        {t.x0, t.y0}, {t.u0, t.v0}, t.color, 0, 0, FLAG_TEXTURED, 0, 0});
    ctx.vertices.push_back(GuiVertex{
        {t.x1, t.y0}, {t.u1, t.v0}, t.color, 0, 0, FLAG_TEXTURED, 0, 0});
    ctx.vertices.push_back(GuiVertex{
        {t.x1, t.y1}, {t.u1, t.v1}, t.color, 0, 0, FLAG_TEXTURED, 0, 0});
    ctx.vertices.push_back(GuiVertex{
        {t.x0, t.y1}, {t.u0, t.v1}, t.color, 0, 0, FLAG_TEXTURED, 0, 0});
  }

  void appendThickLineQuad(GuiRendererContext& ctx, const ThickLineVerts& ln) {
    ctx.vertices.push_back(GuiVertex{
        {ln.x0 + ln.nx, ln.y0 + ln.ny}, {0, 0}, ln.color, 0, 0, 0, 0, 0});
    ctx.vertices.push_back(GuiVertex{
        {ln.x0 - ln.nx, ln.y0 - ln.ny}, {1, 0}, ln.color, 0, 0, 0, 0, 0});
    ctx.vertices.push_back(GuiVertex{
        {ln.x1 - ln.nx, ln.y1 - ln.ny}, {1, 1}, ln.color, 0, 0, 0, 0, 0});
    ctx.vertices.push_back(GuiVertex{
        {ln.x1 + ln.nx, ln.y1 + ln.ny}, {0, 1}, ln.color, 0, 0, 0, 0, 0});
  }

  /// Append 6 indices for a quad from the last 4 vertices.
  void appendQuadIndices(GuiRendererContext& ctx) {
    auto base = static_cast<uint32_t>(ctx.vertices.size() - 4);
    ctx.indices.push_back(base);
    ctx.indices.push_back(base + 1);
    ctx.indices.push_back(base + 2);
    ctx.indices.push_back(base + 2);
    ctx.indices.push_back(base + 3);
    ctx.indices.push_back(base);
  }

  struct QuadBatchMergeKey {
    /// GPU texture handle for batch key comparison.
    RhiTextureHandle texture;
    /// Start offset in the vertex buffer.
    uint32_t vertex_start;
    /// Start offset in the index buffer.
    uint32_t index_start;
  };

  bool canMergeLastQuadBatch(const GuiRendererContext& ctx,
                             const QuadBatchMergeKey& key) {
    if (ctx.commands.empty()) {
      return false;
    }
    const auto& back = ctx.commands.back();
    if (back.type != DrawCommandType::QUAD_BATCH) {
      return false;
    }
    if (back.batch.texture != key.texture) {
      return false;
    }
    return back.batch.vertex_offset + back.batch.vertex_count ==
               key.vertex_start &&
           back.batch.index_offset + back.batch.index_count == key.index_start;
  }

  void extendLastQuadBatch(GuiRendererContext& ctx) {
    auto& back = ctx.commands.back();
    back.batch.vertex_count += 4;
    back.batch.index_count += 6;
  }

  void pushQuadBatchCommand(GuiRendererContext& ctx,
                            const QuadBatchMergeKey& b) {
    DrawCommand cmd{};
    cmd.type = DrawCommandType::QUAD_BATCH;
    cmd.batch.vertex_offset = b.vertex_start;
    cmd.batch.vertex_count = 4;
    cmd.batch.index_offset = b.index_start;
    cmd.batch.index_count = 6;
    cmd.batch.texture = b.texture;
    ctx.commands.push_back(cmd);
  }

  void recordQuadBatch(GuiRendererContext& ctx, RhiTextureHandle texture) {
    const auto v_off = static_cast<uint32_t>(ctx.vertices.size() - 4);
    const auto i_off = static_cast<uint32_t>(ctx.indices.size() - 6);
    QuadBatchMergeKey key{texture, v_off, i_off};
    if (canMergeLastQuadBatch(ctx, key)) {
      extendLastQuadBatch(ctx);
      return;
    }
    pushQuadBatchCommand(ctx, key);
  }

  /// Create a host-visible GPU buffer.
  uint64_t createDynamicBuffer(RhiDevice& device, uint64_t size,
                               RhiBufferUsage usage, const char* name) {
    RhiBufferDesc desc{};
    desc.size = size;
    desc.usage = usage;
    desc.host_visible = true;
    desc.debug_name = name;
    return device.createBuffer(desc);
  }

  /// Per-frame scale from layout coordinates to clip space (GUI VS, buffer 1).
  struct GuiNdcScale {
    /// Horizontal scale `2 / layout_width`.
    float scale_x = 0;
    /// Vertical scale `2 / layout_height`.
    float scale_y = 0;
  };

  /// Upload CPU data to a mapped GPU buffer.
  void uploadBuffer(RhiDevice& device, uint64_t buffer, const void* data,
                    uint64_t size) {
    void* mapped = device.mapBuffer(buffer);
    if (mapped != nullptr) {
      std::memcpy(mapped, data, size);
      device.unmapBuffer(buffer);
    }
  }

  /// Grow a buffer to at least the required capacity (2x strategy).
  /// Parameters for growing a GPU buffer.
  struct GrowBufferParams {
    /// Old GPU buffer handle.
    uint64_t old_buffer;
    /// Old capacity in bytes.
    uint64_t old_cap_bytes;
    /// Required capacity in bytes.
    uint64_t required_bytes;
    /// Buffer usage flags.
    RhiBufferUsage usage;
    /// Debug name for the buffer.
    const char* name;
  };

  uint64_t growBuffer(RhiDevice& device, const GrowBufferParams& p) {
    uint64_t old_buffer = p.old_buffer;
    uint64_t old_cap_bytes = p.old_cap_bytes;
    uint64_t required_bytes = p.required_bytes;
    RhiBufferUsage usage = p.usage;
    const char* name = p.name;
    device.destroyBuffer(old_buffer);
    auto new_size = std::max(old_cap_bytes * 2, required_bytes);
    return createDynamicBuffer(device, new_size, usage, name);
  }

  /// Grow vertex buffer if the frame exceeds current capacity.
  void growVertexBufferIfNeeded(GuiRendererContext& ctx, uint32_t vert_count) {
    if (vert_count <= ctx.vertex_capacity) {
      return;
    }
    auto required = static_cast<uint64_t>(vert_count) * gui::GUI_VERTEX_STRIDE;
    auto old_cap =
        static_cast<uint64_t>(ctx.vertex_capacity) * gui::GUI_VERTEX_STRIDE;
    ctx.vertex_buffer =
        growBuffer(*ctx.device, {ctx.vertex_buffer, old_cap, required,
                                 RhiBufferUsage::VERTEX, "gui_vertex_buffer"});
    ctx.vertex_capacity = vert_count * 2;
  }

  /// Grow index buffer if the frame exceeds current capacity.
  void growIndexBufferIfNeeded(GuiRendererContext& ctx, uint32_t idx_count) {
    if (idx_count <= ctx.index_capacity) {
      return;
    }
    auto required = static_cast<uint64_t>(idx_count) * sizeof(uint32_t);
    auto old_cap = static_cast<uint64_t>(ctx.index_capacity) * sizeof(uint32_t);
    ctx.index_buffer =
        growBuffer(*ctx.device, {ctx.index_buffer, old_cap, required,
                                 RhiBufferUsage::INDEX, "gui_index_buffer"});
    ctx.index_capacity = idx_count * 2;
  }

  /// Upload vertex/index data and bind pipeline + buffers.
  void uploadAndBind(GuiRendererContext& ctx, RhiCommandList& cmd_list,
                     uint32_t vert_count, uint32_t idx_count) {
    uploadBuffer(*ctx.device, ctx.vertex_buffer, ctx.vertices.data(),
                 static_cast<uint64_t>(vert_count) * gui::GUI_VERTEX_STRIDE);
    uploadBuffer(*ctx.device, ctx.index_buffer, ctx.indices.data(),
                 static_cast<uint64_t>(idx_count) * sizeof(uint32_t));
    if (ctx.pipeline != RHI_PIPELINE_INVALID) {
      cmd_list.bindPipeline(ctx.pipeline);
    }
    cmd_list.bindVertexBuffer(ctx.vertex_buffer);
    cmd_list.bindIndexBuffer(ctx.index_buffer, 0, RhiIndexType::UINT32);
    GuiNdcScale ndc{};
    const auto vw = std::max(1u, ctx.viewport_width);
    const auto vh = std::max(1u, ctx.viewport_height);
    ndc.scale_x = 2.0f / static_cast<float>(vw);
    ndc.scale_y = 2.0f / static_cast<float>(vh);
    cmd_list.setVertexStageBytes(&ndc, sizeof(ndc), 1);
  }

  /// Create initial vertex and index buffers on the GPU.
  void createInitialBuffers(GuiRendererContext& ctx, RhiDevice& device) {
    auto vb_bytes =
        static_cast<uint64_t>(DEFAULT_VERTEX_CAPACITY) * gui::GUI_VERTEX_STRIDE;
    ctx.vertex_buffer = createDynamicBuffer(
        device, vb_bytes, RhiBufferUsage::VERTEX, "gui_vertex_buffer");
    ctx.vertex_capacity = DEFAULT_VERTEX_CAPACITY;

    auto ib_bytes =
        static_cast<uint64_t>(DEFAULT_INDEX_CAPACITY) * sizeof(uint32_t);
    ctx.index_buffer = createDynamicBuffer(
        device, ib_bytes, RhiBufferUsage::INDEX, "gui_index_buffer");
    ctx.index_capacity = DEFAULT_INDEX_CAPACITY;
  }

  /// Submit a quad batch draw command.
  void submitQuadBatch(const DrawCommand& cmd, RhiCommandList& cmd_list) {
    cmd_list.bindFragmentTexture(cmd.batch.texture, 0);
    RhiDrawIndexedParams params{};
    params.index_count = cmd.batch.index_count;
    params.first_index = cmd.batch.index_offset;
    params.vertex_offset = static_cast<int32_t>(cmd.batch.vertex_offset);
    cmd_list.drawIndexed(params);
  }

  /// Submit a scissor push/pop command.
  void submitScissorCommand(const DrawCommand& cmd, RhiCommandList& cmd_list,
                            const GuiRendererContext& ctx) {
    const uint32_t sw = std::max(1u, effectiveSurfaceW(ctx));
    const uint32_t sh = std::max(1u, effectiveSurfaceH(ctx));
    if (cmd.type == DrawCommandType::PUSH_SCISSOR) {
      const uint32_t cw = std::max(1u, ctx.viewport_width);
      const uint32_t ch = std::max(1u, ctx.viewport_height);
      const ScissorCmd ms = mapScissorToSurface(cmd.scissor, {cw, ch, sw, sh});
      cmd_list.setScissor(
          {static_cast<int32_t>(ms.x), static_cast<int32_t>(ms.y),
           static_cast<uint32_t>(ms.w), static_cast<uint32_t>(ms.h)});
    } else {
      cmd_list.setScissor({0, 0, sw, sh});
    }
  }

  /// Submit a single draw command to the RHI command list.
  void submitDrawCommand(const DrawCommand& cmd, RhiCommandList& cmd_list,
                         const GuiRendererContext& ctx) {
    if (cmd.type == DrawCommandType::QUAD_BATCH) {
      submitQuadBatch(cmd, cmd_list);
    } else {
      submitScissorCommand(cmd, cmd_list, ctx);
    }
  }

  void submitAllDrawCommands(GuiRendererContext& ctx,
                             RhiCommandList& cmd_list) {
    for (const auto& cmd : ctx.commands) {
      submitDrawCommand(cmd, cmd_list, ctx);
    }
  }

  void drawFullMeshFallback(RhiCommandList& cmd_list, uint32_t idx_count) {
    RhiDrawIndexedParams params{};
    params.index_count = idx_count;
    params.first_index = 0;
    params.vertex_offset = 0;
    cmd_list.drawIndexed(params);
  }

  /// Set full-viewport scissor and viewport on the command list.
  void setFullViewport(const GuiRendererContext& ctx,
                       RhiCommandList& cmd_list) {
    const uint32_t sw = std::max(1u, effectiveSurfaceW(ctx));
    const uint32_t sh = std::max(1u, effectiveSurfaceH(ctx));
    cmd_list.setViewport({0.0f, 0.0f, static_cast<float>(sw),
                          static_cast<float>(sh), 0.0f, 1.0f});
    cmd_list.setScissor({0, 0, sw, sh});
  }

}  // namespace

bool GuiRendererContext::init(RhiDevice* dev) {
  scissor_stack.depth = 0;
  device = dev;
  if (dev == nullptr) {
    return true;
  }
  createInitialBuffers(*this, *dev);
  pipeline = RHI_PIPELINE_INVALID;
  if (dev->tryCreateGuiPipeline(pipeline)) {
    return true;
  }
  RhiGraphicsPipelineDesc pipe_desc{};
  pipeline = dev->createGraphicsPipeline(pipe_desc);
  return true;
}

void GuiRendererContext::shutdown() {
  if (device == nullptr) {
    return;
  }

  if (vertex_buffer != 0) {
    device->destroyBuffer(vertex_buffer);
  }
  if (index_buffer != 0) {
    device->destroyBuffer(index_buffer);
  }
  if (pipeline != 0) {
    device->destroyPipeline(pipeline);
  }

  vertex_buffer = 0;
  index_buffer = 0;
  pipeline = 0;
  device = nullptr;
}

void GuiRendererContext::beginFrame() {
  vertices.clear();
  indices.clear();
  commands.clear();
}

void GuiRendererContext::emitQuad(const EmitQuadParams& params) {
  uint32_t flags = (params.corner_radius > 0.0f) ? FLAG_ROUNDED_RECT : 0;
  StyledQuadVerts q{params.rect.x,
                    params.rect.y,
                    params.rect.x + params.rect.w,
                    params.rect.y + params.rect.h,
                    params.color,
                    params.corner_radius,
                    params.border_width,
                    flags};
  appendStyledQuadVertices(*this, q);
  appendQuadIndices(*this);
  recordQuadBatch(*this, 0);
}

void GuiRendererContext::emitTexturedQuad(
    const EmitTexturedQuadParams& params) {
  TexturedQuadVerts t{params.rect.x,
                      params.rect.y,
                      params.rect.x + params.rect.w,
                      params.rect.y + params.rect.h,
                      params.uv.x,
                      params.uv.y,
                      params.uv.x + params.uv.w,
                      params.uv.y + params.uv.h,
                      params.color};
  appendTexturedQuadCorners(*this, t);
  appendQuadIndices(*this);
  recordQuadBatch(*this, params.texture);
}

void GuiRendererContext::emitGlyph(const EmitGlyphParams& params) {
  if (params.glyph.atlas_w == 0 || params.glyph.atlas_h == 0) {
    return;
  }
  if (params.glyph.atlas_texture == RHI_TEXTURE_INVALID) {
    return;
  }
  auto gr = computeGlyphRect(params.x, params.y, params.glyph, params.scale);
  Rect screen{gr.x, gr.y, gr.w, gr.h};
  Rect uv{params.glyph.uv_u0, params.glyph.uv_v0,
          params.glyph.uv_u1 - params.glyph.uv_u0,
          params.glyph.uv_v1 - params.glyph.uv_v0};
  emitTexturedQuad({screen, uv, params.glyph.atlas_texture, params.color});
}

void GuiRendererContext::emitLine(const EmitLineParams& params) {
  float dx = params.x1 - params.x0;
  float dy = params.y1 - params.y0;
  float len = std::sqrt(dx * dx + dy * dy);
  if (len < MIN_LINE_LENGTH) {
    return;
  }
  float half = params.width * 0.5f;
  float nx = -dy / len * half;
  float ny = dx / len * half;
  ThickLineVerts ln{params.x0, params.y0, params.x1,   params.y1,
                    nx,        ny,        params.color};
  appendThickLineQuad(*this, ln);
  appendQuadIndices(*this);
  recordQuadBatch(*this, 0);
}

void GuiRendererContext::pushScissor(const Rect& rect) {
  if (scissor_stack.depth >= MAX_SCISSOR_DEPTH) {
    return;
  }
  auto d = scissor_stack.depth;
  scissor_stack.rects[d][0] = rect.x;
  scissor_stack.rects[d][1] = rect.y;
  scissor_stack.rects[d][2] = rect.w;
  scissor_stack.rects[d][3] = rect.h;
  scissor_stack.depth++;
}

void GuiRendererContext::popScissor() {
  if (scissor_stack.depth > 0) {
    scissor_stack.depth--;
  }
}

void GuiRendererContext::endFrame(RhiCommandList& cmd_list) {
  if (vertices.empty() || device == nullptr) {
    return;
  }

  const auto vert_count = static_cast<uint32_t>(vertices.size());
  const auto idx_count = static_cast<uint32_t>(indices.size());
  growVertexBufferIfNeeded(*this, vert_count);
  growIndexBufferIfNeeded(*this, idx_count);
  uploadAndBind(*this, cmd_list, vert_count, idx_count);
  setFullViewport(*this, cmd_list);

  if (!commands.empty()) {
    submitAllDrawCommands(*this, cmd_list);
    return;
  }
  drawFullMeshFallback(cmd_list, idx_count);
}

}  // namespace eng
