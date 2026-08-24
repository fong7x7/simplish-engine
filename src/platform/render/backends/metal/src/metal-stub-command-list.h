#pragma once

#ifdef ENGINE_RENDERER_METAL

#include <engine/render/rhi-command-list.h>

namespace eng {

/// Stub command list: all recording operations are no-ops.
/// Main thread only.
class MetalStubCommandList final : public RhiCommandList {
public:
  void begin() override {}
  void end() override {}
  void beginRenderPass(const RhiRenderPassBeginInfo& /*info*/) override {}
  void endRenderPass() override {}
  void bindPipeline(RhiPipelineHandle /*handle*/) override {}
  void bindVertexBuffer(RhiBufferHandle /*handle*/,
                        uint64_t /*offset*/) override {}
  void bindIndexBuffer(RhiBufferHandle /*handle*/, uint64_t /*offset*/,
                       RhiIndexType /*type*/) override {}
  void bindDescriptorSet(uint32_t /*set*/,
                         RhiDescriptorSetHandle /*handle*/) override {}
  void setViewport(const RhiViewport& /*viewport*/) override {}
  void setScissor(const RhiScissor& /*scissor*/) override {}
  void draw(const RhiDrawParams& /*params*/) override {}
  void drawIndexed(const RhiDrawIndexedParams& /*params*/) override {}
  void dispatch(uint32_t /*x*/, uint32_t /*y*/, uint32_t /*z*/) override {}
  void drawIndirect(const RhiDrawIndirectParams& /*params*/) override {}
  void
  drawIndexedIndirect(const RhiDrawIndexedIndirectParams& /*params*/) override {
  }
  void drawIndexedIndirectCount(
      const RhiDrawIndexedIndirectCountParams& /*params*/) override {}
  void copyBuffer(const RhiCopyBufferParams& /*params*/) override {}
  void copyTextureToBuffer(RhiTextureHandle /*src*/,
                           RhiBufferHandle /*dst*/) override {}
  void textureBarrier(RhiTextureHandle /*handle*/,
                      RhiTextureLayout /*old_layout*/,
                      RhiTextureLayout /*new_layout*/) override {}
  void bindComputeStorageImage(RhiTextureHandle /*texture*/,
                               uint32_t /*mip_level*/,
                               uint32_t /*slot*/) override {}
  void bindComputeSampledTexture(RhiTextureHandle /*texture*/,
                                 uint32_t /*slot*/) override {}
  void computeBarrier() override {}
  void bufferBarrier(RhiBufferHandle /*buffer*/, RhiBarrierStage /*src_stage*/,
                     RhiBarrierStage /*dst_stage*/) override {}
};

}  // namespace eng

#endif  // ENGINE_RENDERER_METAL
