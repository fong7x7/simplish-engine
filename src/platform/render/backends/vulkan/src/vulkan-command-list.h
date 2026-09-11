#pragma once

#ifdef ENGINE_RENDERER_VULKAN

#include "vulkan-device-impl.h"
#include "vulkan-image-ref.h"
#include "vulkan-pass-attachments.h"
#include "vulkan-stage-bindings.h"

#include <engine/render/rhi-command-list.h>
#include <vulkan/vulkan.h>

namespace eng::render {

// ============================================================================
// DESIGN SUMMARY
// ============================================================================
// VulkanCommandList: Vulkan implementation of the RhiCommandList interface.
//
// Responsibilities:
// - Wrap a borrowed VkCommandBuffer for GPU command recording
// - Translate RHI commands (bind, draw, dispatch, barrier) to vkCmd* calls
// - Manage render pass begin/end via VK_KHR_dynamic_rendering
// - Turn Metal-style stage bytes and fragment textures into one push
//   descriptor set, pushed at the draw after any of them changes
// - Move images between layouts the renderers never name: attachments into
//   their attachment layouts at a pass's start, sampled ones back to the
//   shader-read layout at its end, and the back buffer to PRESENT at end()
//
// Key Invariants:
// - Does NOT own the VkCommandBuffer (lifetime managed by PerFrameData)
// - Must be used between begin()/end() calls
// - draw* is skipped outside a render pass or with no graphics pipeline
//   bound, as the Metal backend skips it without an encoder or a PSO
// - Outside a pass, every sampled texture is in the shader-read layout
// - All methods are main-thread-only
//
// Threading:
// - Recording is single-threaded (main thread)
// - The underlying command buffer is submitted to the GPU queue separately
// ============================================================================

class VulkanCommandList final : public RhiCommandList {
public:
  /// Constructs a command list wrapping a borrowed VkCommandBuffer.
  VulkanCommandList(VkCommandBuffer cmd_buffer, VulkanDevice::Impl& impl);

  ~VulkanCommandList() override = default;

  // --- Recording lifecycle ---
  void begin() override;
  void end() override;

  // --- Render pass ---
  void beginRenderPass(const RhiRenderPassBeginInfo& info) override;
  void endRenderPass() override;

  // --- Pipeline binding ---
  void bindPipeline(RhiPipelineHandle pipeline) override;

  // --- Resource binding ---
  void bindVertexBuffer(RhiBufferHandle buffer, uint64_t offset = 0) override;
  void bindIndexBuffer(RhiBufferHandle buffer, uint64_t offset = 0,
                       RhiIndexType index_type = RhiIndexType::UINT16) override;
  void bindDescriptorSet(uint32_t set_index,
                         RhiDescriptorSetHandle set) override;

  // --- Metal-style stage bytes and texture binding ---
  void setVertexStageBytes(const void* data, size_t size,
                           uint32_t slot) override;
  void setFragmentStageBytes(const void* data, size_t size,
                             uint32_t slot) override;
  void bindFragmentTexture(RhiTextureHandle texture, uint32_t slot) override;

  // --- Viewport and scissor ---
  void setViewport(const RhiViewport& viewport) override;
  void setScissor(const RhiScissor& scissor) override;

  // --- Draw commands ---
  void draw(const RhiDrawParams& params) override;
  void drawIndexed(const RhiDrawIndexedParams& params) override;
  void drawIndirect(const RhiDrawIndirectParams& params) override;
  void drawIndexedIndirect(const RhiDrawIndexedIndirectParams& params) override;
  void drawIndexedIndirectCount(
      const RhiDrawIndexedIndirectCountParams& params) override;

  // --- Compute dispatch ---
  void dispatch(uint32_t groups_x, uint32_t groups_y = 1,
                uint32_t groups_z = 1) override;

  // --- Copy commands ---
  void copyBuffer(const RhiCopyBufferParams& params) override;
  void copyTextureToBuffer(RhiTextureHandle src, RhiBufferHandle dst) override;

  // --- Compute texture binding ---
  void bindComputeStorageImage(RhiTextureHandle texture, uint32_t mip_level,
                               uint32_t slot) override;
  void bindComputeSampledTexture(RhiTextureHandle texture,
                                 uint32_t slot) override;

  // --- Barriers ---
  void textureBarrier(RhiTextureHandle texture, RhiTextureLayout old_layout,
                      RhiTextureLayout new_layout) override;
  void computeBarrier() override;
  void bufferBarrier(RhiBufferHandle buffer, RhiBarrierStage src_stage,
                     RhiBarrierStage dst_stage) override;

  /// Returns the underlying VkCommandBuffer (for submit).
  VkCommandBuffer nativeCommandBuffer() const;

private:
  /// Attach the pass's colour targets, each moved to the attachment layout.
  void attachColors(const RhiRenderPassBeginInfo& info);
  /// Attach the pass's depth target, moved to the attachment layout.
  void attachDepth(const RhiRenderPassBeginInfo& info);
  /// Move a target the pass drew into back to its resting layout.
  void settleTarget(RhiTextureHandle texture);
  /// Viewport and scissor over the whole render area, which is what a Metal
  /// encoder starts with; a draw that sets neither still draws.
  void setFullViewport(VkExtent2D extent);
  /// Copy stage bytes into the frame's ring and bind them at `binding`.
  void stageBytes(uint32_t binding, const void* data, size_t size);
  /// The view to bind for `ref`, or null to bind the stand-in instead.
  VkImageView sampleableView(const VulkanImageRef& ref);
  /// Push the stage bindings if they changed since the last push.
  void flushBindings();
  /// Whether a draw can be recorded now; pushes pending bindings if so.
  bool prepareDraw();

  /// Borrowed Vulkan command buffer (not owned).
  VkCommandBuffer cmd_buffer_ = VK_NULL_HANDLE;
  /// Device implementation for resolving RHI handles to Vulkan objects.
  VulkanDevice::Impl& impl_;
  /// Whether a graphics pipeline is bound, so a draw has something to run.
  bool graphics_bound_ = false;
  /// Whether a render pass is open.
  bool in_pass_ = false;
  /// Descriptors the next draw pushes.
  VulkanStageBindings bindings_{};
  /// Attachments of the open pass.
  VulkanPassAttachments pass_{};
};

}  // namespace eng::render

#endif  // ENGINE_RENDERER_VULKAN
