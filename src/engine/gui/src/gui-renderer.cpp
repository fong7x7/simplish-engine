#include <algorithm>
#include <array>
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
#include <iterator>

namespace eng {

/// The whole of a texture, or of a shape's own coordinates.
constexpr Rect FULL_UV{0.0f, 0.0f, 1.0f, 1.0f};
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

  /// A quad's four corners, clockwise from the top left, and the texture
  /// or shape coordinate at each.
  struct QuadCorners {
    /// Layout-space positions.
    std::array<std::array<float, 2>, 4> pos{};
    /// Texture coordinates, or 0..1 across a shape.
    std::array<std::array<float, 2>, 4> uv{};
  };

  /// @p rect's corners, with @p uv's across them.
  QuadCorners rectCorners(const Rect& rect, const Rect& uv) {
    const float x1 = rect.x + rect.w;
    const float y1 = rect.y + rect.h;
    const float u1 = uv.x + uv.w;
    const float v1 = uv.y + uv.h;
    return {{{{rect.x, rect.y}, {x1, rect.y}, {x1, y1}, {rect.x, y1}}},
            {{{uv.x, uv.y}, {u1, uv.y}, {u1, v1}, {uv.x, v1}}}};
  }

  /// The corners of @p line's quad, (@p nx, @p ny) either side of it.
  QuadCorners lineCorners(const GuiRendererContext::EmitLineParams& line,
                          float nx, float ny) {
    return {{{{line.x0 + nx, line.y0 + ny},
              {line.x0 - nx, line.y0 - ny},
              {line.x1 - nx, line.y1 - ny},
              {line.x1 + nx, line.y1 + ny}}},
            {{{0.0f, 0.0f}, {1.0f, 0.0f}, {1.0f, 1.0f}, {0.0f, 1.0f}}}};
  }

  /// @p packed with its alpha scaled by @p factor.
  uint32_t scaleAlpha(uint32_t packed, float factor) {
    if (factor >= 1.0f) {
      return packed;
    }
    const auto alpha = static_cast<float>(packed >> 24U) * factor;
    return (packed & 0x00FFFFFFU) |
           (static_cast<uint32_t>(std::clamp(alpha, 0.0f, 255.0f)) << 24U);
  }

  /// @p v's lengths — quad size, radii, borders, blur — scaled by @p s,
  /// so a shape drawn scaled keeps its proportions.
  void scaleLengths(GuiVertex& v, float s) {
    v.rect_w *= s;
    v.rect_h *= s;
    for (size_t i = 0; i < 4; ++i) {
      v.radii[i] *= s;
      v.border[i] *= s;
    }
    if ((v.flags & GUI_VERTEX_LINEAR_GRADIENT) == 0) {
      v.param *= s;
    }
  }

  /// @p v as the context's transform and alpha scale draw it.
  GuiVertex transformed(const GuiRendererContext& ctx, GuiVertex v) {
    const GuiRenderTransform& t = ctx.transform;
    v.pos[0] = t.mapX(v.pos[0]);
    v.pos[1] = t.mapY(v.pos[1]);
    if (t.scale != 1.0f) {
      scaleLengths(v, t.scale);
    }
    v.color = scaleAlpha(v.color, ctx.alpha_scale);
    v.color2 = scaleAlpha(v.color2, ctx.alpha_scale);
    return v;
  }

  /// Append the four corners of a quad sharing @p shared's fields.
  void appendCorners(GuiRendererContext& ctx, const GuiVertex& shared,
                     const QuadCorners& corners) {
    for (size_t i = 0; i < 4; ++i) {
      GuiVertex v = shared;
      v.pos[0] = corners.pos[i][0];
      v.pos[1] = corners.pos[i][1];
      v.uv[0] = corners.uv[i][0];
      v.uv[1] = corners.uv[i][1];
      ctx.vertices.push_back(transformed(ctx, v));
    }
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

  /// Grow a pair's vertex buffer if the frame exceeds its capacity. The
  /// other pairs keep theirs, and grow only when a frame needs them to.
  void growVertexBufferIfNeeded(RhiDevice& device, GuiFrameBuffers& pair,
                                uint32_t vert_count) {
    if (vert_count <= pair.vertex_capacity) {
      return;
    }
    auto required = static_cast<uint64_t>(vert_count) * gui::GUI_VERTEX_STRIDE;
    auto old_cap =
        static_cast<uint64_t>(pair.vertex_capacity) * gui::GUI_VERTEX_STRIDE;
    pair.vertex_buffer =
        growBuffer(device, {pair.vertex_buffer, old_cap, required,
                            RhiBufferUsage::VERTEX, "gui_vertex_buffer"});
    pair.vertex_capacity = vert_count * 2;
  }

  /// Grow a pair's index buffer if the frame exceeds its capacity.
  void growIndexBufferIfNeeded(RhiDevice& device, GuiFrameBuffers& pair,
                               uint32_t idx_count) {
    if (idx_count <= pair.index_capacity) {
      return;
    }
    auto required = static_cast<uint64_t>(idx_count) * sizeof(uint32_t);
    auto old_cap =
        static_cast<uint64_t>(pair.index_capacity) * sizeof(uint32_t);
    pair.index_buffer =
        growBuffer(device, {pair.index_buffer, old_cap, required,
                            RhiBufferUsage::INDEX, "gui_index_buffer"});
    pair.index_capacity = idx_count * 2;
  }

  /// Copy the frame's vertices and indices into `pair`.
  void uploadFrameBuffers(GuiRendererContext& ctx, const GuiFrameBuffers& pair,
                          uint32_t vert_count, uint32_t idx_count) {
    uploadBuffer(*ctx.device, pair.vertex_buffer, ctx.vertices.data(),
                 static_cast<uint64_t>(vert_count) * gui::GUI_VERTEX_STRIDE);
    uploadBuffer(*ctx.device, pair.index_buffer, ctx.indices.data(),
                 static_cast<uint64_t>(idx_count) * sizeof(uint32_t));
  }

  void bindFrameState(GuiRendererContext& ctx, RhiCommandList& cmd_list) {
    if (ctx.pipeline != RHI_PIPELINE_INVALID) {
      cmd_list.bindPipeline(ctx.pipeline);
    }
    const GuiFrameBuffers& pair = ctx.frame_buffers[ctx.frame_slot];
    cmd_list.bindVertexBuffer(pair.vertex_buffer);
    cmd_list.bindIndexBuffer(pair.index_buffer, 0, RhiIndexType::UINT32);
    GuiNdcScale ndc{};
    const auto vw = std::max(1u, ctx.viewport_width);
    const auto vh = std::max(1u, ctx.viewport_height);
    ndc.scale_x = 2.0f / static_cast<float>(vw);
    ndc.scale_y = 2.0f / static_cast<float>(vh);
    cmd_list.setVertexStageBytes(&ndc, sizeof(ndc), 1);
  }

  /// Create one pair's vertex and index buffers at the default capacities.
  GuiFrameBuffers createInitialBuffers(RhiDevice& device) {
    auto vb_bytes =
        static_cast<uint64_t>(DEFAULT_VERTEX_CAPACITY) * gui::GUI_VERTEX_STRIDE;
    auto ib_bytes =
        static_cast<uint64_t>(DEFAULT_INDEX_CAPACITY) * sizeof(uint32_t);
    GuiFrameBuffers pair{};
    pair.vertex_buffer = createDynamicBuffer(
        device, vb_bytes, RhiBufferUsage::VERTEX, "gui_vertex_buffer");
    pair.vertex_capacity = DEFAULT_VERTEX_CAPACITY;
    pair.index_buffer = createDynamicBuffer(
        device, ib_bytes, RhiBufferUsage::INDEX, "gui_index_buffer");
    pair.index_capacity = DEFAULT_INDEX_CAPACITY;
    return pair;
  }

  /// Destroy one pair's buffers and forget them.
  void destroyFrameBuffers(RhiDevice& device, GuiFrameBuffers& pair) {
    if (pair.vertex_buffer != 0) {
      device.destroyBuffer(pair.vertex_buffer);
    }
    if (pair.index_buffer != 0) {
      device.destroyBuffer(pair.index_buffer);
    }
    pair = GuiFrameBuffers{};
  }

  /// Submit a quad batch draw command.
  ///
  /// `appendQuadIndices` writes each index as the vertex's position in the
  /// whole frame, so the batch is selected by its first index alone. A
  /// vertex offset as well would count the batch's start twice: every batch
  /// after the first would draw the quads that follow it, dropping its own
  /// first quad and reading past its last. Metal and OpenGL each used to
  /// ignore one of the two offsets, which hid that; Vulkan and DX12 honour
  /// both, as the RHI says to.
  void submitQuadBatch(const DrawCommand& cmd, RhiCommandList& cmd_list) {
    cmd_list.bindFragmentTexture(cmd.batch.texture, 0);
    RhiDrawIndexedParams params{};
    params.index_count = cmd.batch.index_count;
    params.first_index = cmd.batch.index_offset;
    params.vertex_offset = 0;
    cmd_list.drawIndexed(params);
  }

  /// Submit a scissor push/pop command.
  ///
  /// A pop carries the rect to restore — the enclosing scissor when one is
  /// still on the stack — and an empty rect means "back to the whole
  /// surface". A push always applies, even when its rect is degenerate,
  /// because an empty clip must hide its contents rather than reveal them.
  void submitScissorCommand(const DrawCommand& cmd, RhiCommandList& cmd_list,
                            const GuiRendererContext& ctx) {
    const uint32_t sw = std::max(1u, effectiveSurfaceW(ctx));
    const uint32_t sh = std::max(1u, effectiveSurfaceH(ctx));
    const bool restore_full = cmd.type == DrawCommandType::POP_SCISSOR &&
                              (cmd.scissor.w <= 0.0f || cmd.scissor.h <= 0.0f);
    if (restore_full) {
      cmd_list.setScissor({0, 0, sw, sh});
      return;
    }
    const uint32_t cw = std::max(1u, ctx.viewport_width);
    const uint32_t ch = std::max(1u, ctx.viewport_height);
    const ScissorCmd ms = mapScissorToSurface(cmd.scissor, {cw, ch, sw, sh});
    cmd_list.setScissor({static_cast<int32_t>(ms.x), static_cast<int32_t>(ms.y),
                         static_cast<uint32_t>(ms.w),
                         static_cast<uint32_t>(ms.h)});
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

  void drawFullMeshFallback(RhiCommandList& cmd_list, uint32_t idx_count) {
    RhiDrawIndexedParams params{};
    params.index_count = idx_count;
    params.first_index = 0;
    params.vertex_offset = 0;
    cmd_list.drawIndexed(params);
  }

  /// Read one scissor-stack entry as a Rect.
  Rect scissorRectAt(const ScissorStack& stack, uint32_t index) {
    return {stack.rects[index][0], stack.rects[index][1], stack.rects[index][2],
            stack.rects[index][3]};
  }

  /// Store a rect at the top of the scissor stack.
  void storeScissorRect(ScissorStack& stack, uint32_t index, const Rect& rect) {
    stack.rects[index][0] = rect.x;
    stack.rects[index][1] = rect.y;
    stack.rects[index][2] = rect.w;
    stack.rects[index][3] = rect.h;
  }

  /// Append a scissor command to the draw stream.
  ///
  /// The stack alone only tracks state; without a command in the stream
  /// nothing ever reaches `RhiCommandList::setScissor`, and clipped content
  /// paints over the rest of the frame.
  void appendScissorCommand(GuiRendererContext& ctx, DrawCommandType type,
                            const Rect& rect) {
    DrawCommand cmd{};
    cmd.type = type;
    cmd.scissor = {rect.x, rect.y, rect.w, rect.h};
    ctx.commands.push_back(cmd);
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
  for (GuiFrameBuffers& pair : frame_buffers) {
    pair = createInitialBuffers(*dev);
  }
  frame_slot = 0;
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

  for (GuiFrameBuffers& pair : frame_buffers) {
    destroyFrameBuffers(*device, pair);
  }
  if (pipeline != 0) {
    device->destroyPipeline(pipeline);
  }
  frame_slot = 0;
  pipeline = 0;
  device = nullptr;
}

void GuiRendererContext::beginFrame() {
  vertices.clear();
  indices.clear();
  commands.clear();
  scene_split = NO_SCENE_SPLIT;
  // The stack has to be cleared with the stream it describes. A widget that
  // returns between pushScissor and popScissor would otherwise leave a clip
  // on the stack that narrows every later frame.
  scissor_stack.depth = 0;
  transform = {};
  alpha_scale = 1.0f;
}

void GuiRendererContext::emitQuad(const EmitQuadParams& params) {
  GuiVertex style{.color = params.color, .color2 = params.color};
  if (params.corner_radius > 0.0f) {
    style.flags = GUI_VERTEX_SHAPE;
    std::ranges::fill(style.radii, params.corner_radius);
  }
  if (params.border_width > 0.0f) {
    std::ranges::fill(style.border, params.border_width);
  }
  emitShape(params.rect, style);
}

void GuiRendererContext::emitShape(const Rect& rect, const GuiVertex& style) {
  GuiVertex shared = style;
  shared.rect_w = rect.w;
  shared.rect_h = rect.h;
  appendCorners(*this, shared, rectCorners(rect, FULL_UV));
  appendQuadIndices(*this);
  recordQuadBatch(*this, 0);
}

void GuiRendererContext::emitTexturedQuad(
    const EmitTexturedQuadParams& params) {
  const GuiVertex shared{.color = params.color,
                         .color2 = params.color,
                         .flags = GUI_VERTEX_TEXTURED};
  appendCorners(*this, shared, rectCorners(params.rect, params.uv));
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
  const float dx = params.x1 - params.x0;
  const float dy = params.y1 - params.y0;
  const float len = std::sqrt(dx * dx + dy * dy);
  if (len < MIN_LINE_LENGTH) {
    return;
  }
  const float nx = -dy / len * params.width * 0.5f;
  const float ny = dx / len * params.width * 0.5f;
  appendCorners(*this, {.color = params.color, .color2 = params.color},
                lineCorners(params, nx, ny));
  appendQuadIndices(*this);
  recordQuadBatch(*this, 0);
}

void GuiRendererContext::pushScissor(const Rect& rect) {
  if (scissor_stack.depth >= MAX_SCISSOR_DEPTH) {
    return;
  }
  const auto depth = scissor_stack.depth;
  // Clips are laid-out rects, and move with what they clip. A nested clip
  // can only ever shrink its parent's.
  const Rect drawn = transform.map(rect);
  const Rect clipped =
      depth > 0 ? intersectRects(scissorRectAt(scissor_stack, depth - 1), drawn)
                : drawn;
  storeScissorRect(scissor_stack, depth, clipped);
  scissor_stack.depth++;
  appendScissorCommand(*this, DrawCommandType::PUSH_SCISSOR, clipped);
}

void GuiRendererContext::popScissor() {
  if (scissor_stack.depth == 0) {
    return;
  }
  scissor_stack.depth--;
  const Rect restore =
      scissor_stack.depth > 0
          ? scissorRectAt(scissor_stack, scissor_stack.depth - 1)
          : Rect{};
  appendScissorCommand(*this, DrawCommandType::POP_SCISSOR, restore);
}

void GuiRendererContext::markSceneSplit() {
  scene_split = commands.size();
}

size_t GuiRendererContext::sceneSplit() const {
  return scene_split == NO_SCENE_SPLIT ? commands.size() : scene_split;
}

void GuiRendererContext::uploadFrame() {
  if (vertices.empty() || device == nullptr) {
    return;
  }
  // Every backend keeps frames in flight, so the pair the last frame wrote
  // may still be feeding its draws. Write the next one instead; by the time
  // the rotation comes back round, the frame that used it has retired.
  frame_slot = (frame_slot + 1) % GUI_FRAME_BUFFER_COUNT;
  GuiFrameBuffers& pair = frame_buffers[frame_slot];
  const auto vert_count = static_cast<uint32_t>(vertices.size());
  const auto idx_count = static_cast<uint32_t>(indices.size());
  growVertexBufferIfNeeded(*device, pair, vert_count);
  growIndexBufferIfNeeded(*device, pair, idx_count);
  uploadFrameBuffers(*this, pair, vert_count, idx_count);
}

void GuiRendererContext::bindFrame(RhiCommandList& cmd_list) {
  if (vertices.empty() || device == nullptr) {
    return;
  }
  bindFrameState(*this, cmd_list);
  setFullViewport(*this, cmd_list);
}

void GuiRendererContext::submitCommandRange(RhiCommandList& cmd_list,
                                            size_t first, size_t count) {
  const size_t last = std::min(commands.size(), first + count);
  for (size_t i = first; i < last; ++i) {
    submitDrawCommand(commands[i], cmd_list, *this);
  }
}

void GuiRendererContext::endFrame(RhiCommandList& cmd_list) {
  if (vertices.empty() || device == nullptr) {
    return;
  }
  uploadFrame();
  bindFrame(cmd_list);
  if (!commands.empty()) {
    submitCommandRange(cmd_list, 0, commands.size());
    return;
  }
  drawFullMeshFallback(cmd_list, static_cast<uint32_t>(indices.size()));
}

}  // namespace eng
