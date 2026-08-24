#pragma once

#include "rhi-copy-buffer-params.h"
#include "rhi-draw-indexed-indirect-count-params.h"
#include "rhi-draw-indexed-indirect-params.h"
#include "rhi-draw-indexed-params.h"
#include "rhi-draw-indirect-params.h"
#include "rhi-draw-params.h"
#include "rhi-render-pass-begin-info.h"
#include "rhi-scissor.h"
#include "rhi-types.h"
#include "rhi-viewport.h"

#include <cstddef>
#include <cstdint>

namespace eng {

// ============================================================================
// DESIGN SUMMARY
// ============================================================================
// RhiCommandList: Abstract GPU command recording interface.
//
// Responsibilities:
// - Record rendering commands (bind, draw, dispatch, copy, barrier)
// - Abstract over Vulkan command buffers, DX12 command lists, OpenGL calls
// - Provide render pass begin/end for framebuffer targeting
//
// Key Invariants:
// - Created via RhiDevice::createCommandList()
// - Record commands between begin()/end() calls
// - Submitted via RhiDevice::submit(); must not be reused until next frame
// - All methods are main-thread-only
// - draw/drawIndexed must be inside a render pass
// (beginRenderPass..endRenderPass)
// - dispatch must be outside a render pass
// - Pipeline must be bound before draw/dispatch
// - Optional: setVertexStageBytes / bindFragmentTexture before draw (backend-
//   specific; defaults are no-ops)
// ============================================================================

class RhiCommandList {
public:
  virtual ~RhiCommandList() = default;

  // --- Recording lifecycle ---
  virtual void begin() = 0;
  virtual void end() = 0;

  // --- Render pass ---
  virtual void beginRenderPass(const RhiRenderPassBeginInfo& info) = 0;
  virtual void endRenderPass() = 0;

  // --- Pipeline binding ---
  virtual void bindPipeline(RhiPipelineHandle pipeline) = 0;

  // --- Resource binding ---
  virtual void bindVertexBuffer(RhiBufferHandle buffer,
                                uint64_t offset = 0) = 0;
  virtual void
  bindIndexBuffer(RhiBufferHandle buffer, uint64_t offset = 0,
                  RhiIndexType index_type = RhiIndexType::UINT16) = 0;
  virtual void bindDescriptorSet(uint32_t set_index,
                                 RhiDescriptorSetHandle set) = 0;

  /// Push small vertex-stage constants (Metal: setVertexBytes). Default no-op.
  virtual void setVertexStageBytes(const void* data, size_t size,
                                   uint32_t slot);

  /// Bind a sampled texture for the fragment stage at `slot`. Default no-op.
  virtual void bindFragmentTexture(RhiTextureHandle texture, uint32_t slot);

  /// Bind a storage buffer at `slot` for shader read. Default no-op.
  virtual void bindStorageBuffer(RhiBufferHandle buffer, uint32_t slot);

  // --- Viewport and scissor ---
  virtual void setViewport(const RhiViewport& viewport) = 0;
  virtual void setScissor(const RhiScissor& scissor) = 0;

  // --- Draw commands (inside render pass) ---
  virtual void draw(const RhiDrawParams& params) = 0;
  virtual void drawIndexed(const RhiDrawIndexedParams& params) = 0;

  /// Execute non-indexed draws from an indirect buffer.
  /// Default: no-op (unsupported on OpenGL / Metal stub).
  virtual void drawIndirect(const RhiDrawIndirectParams& params);

  /// Execute indexed draws from an indirect buffer.
  /// Default: no-op (unsupported on OpenGL / Metal stub).
  virtual void drawIndexedIndirect(const RhiDrawIndexedIndirectParams& params);

  /// GPU-driven indexed indirect draw with count read from a buffer.
  /// Default: no-op (unsupported on OpenGL / Metal stub).
  virtual void
  drawIndexedIndirectCount(const RhiDrawIndexedIndirectCountParams& params);

  // --- Compute dispatch (outside render pass) ---
  virtual void dispatch(uint32_t groups_x, uint32_t groups_y = 1,
                        uint32_t groups_z = 1) = 0;

  // --- Copy commands ---
  virtual void copyBuffer(const RhiCopyBufferParams& params) = 0;
  virtual void copyTextureToBuffer(RhiTextureHandle src,
                                   RhiBufferHandle dst) = 0;

  // --- Barriers ---
  virtual void textureBarrier(RhiTextureHandle texture,
                              RhiTextureLayout old_layout,
                              RhiTextureLayout new_layout) = 0;

  /// Bind a texture mip level as a storage image for compute. Default: no-op.
  virtual void bindComputeStorageImage(RhiTextureHandle texture,
                                       uint32_t mip_level, uint32_t slot);

  /// Bind a texture as a sampled image for compute. Default: no-op.
  virtual void bindComputeSampledTexture(RhiTextureHandle texture,
                                         uint32_t slot);

  /// Insert a full execution + memory barrier between compute dispatches.
  /// Default: no-op (unsupported on OpenGL / Metal stub).
  virtual void computeBarrier();

  /// Insert an execution + memory barrier between buffer usages.
  /// Default: no-op (unsupported on OpenGL / Metal stub).
  virtual void bufferBarrier(RhiBufferHandle buffer, RhiBarrierStage src_stage,
                             RhiBarrierStage dst_stage);

  // Prevent copying
  RhiCommandList(const RhiCommandList&) = delete;
  RhiCommandList& operator=(const RhiCommandList&) = delete;

protected:
  RhiCommandList() = default;
};

inline void RhiCommandList::setVertexStageBytes(const void* /*data*/,
                                                size_t /*size*/,
                                                uint32_t /*slot*/) {}

inline void RhiCommandList::bindFragmentTexture(RhiTextureHandle /*texture*/,
                                                uint32_t /*slot*/) {}

inline void RhiCommandList::bindStorageBuffer(RhiBufferHandle /*buffer*/,
                                              uint32_t /*slot*/) {}

inline void
RhiCommandList::drawIndirect(const RhiDrawIndirectParams& /*params*/) {}

inline void RhiCommandList::drawIndexedIndirect(
    const RhiDrawIndexedIndirectParams& /*params*/) {}

inline void RhiCommandList::drawIndexedIndirectCount(
    const RhiDrawIndexedIndirectCountParams& /*params*/) {}

inline void RhiCommandList::bindComputeStorageImage(
    RhiTextureHandle /*texture*/, uint32_t /*mip_level*/, uint32_t /*slot*/) {}

inline void
RhiCommandList::bindComputeSampledTexture(RhiTextureHandle /*texture*/,
                                          uint32_t /*slot*/) {}

inline void RhiCommandList::computeBarrier() {}

inline void RhiCommandList::bufferBarrier(RhiBufferHandle /*buffer*/,
                                          RhiBarrierStage /*src_stage*/,
                                          RhiBarrierStage /*dst_stage*/) {}

}  // namespace eng
