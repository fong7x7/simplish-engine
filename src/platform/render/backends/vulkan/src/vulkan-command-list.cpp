#include "vulkan-command-list.h"

#ifdef ENGINE_RENDERER_VULKAN

#include "vulkan-device-impl.h"
#include "vulkan-format-map.h"

#include <cstdint>
#include <engine/render/rhi-types.h>
#include <vulkan/vulkan.h>

namespace eng::render {

// ---------------------------------------------------------------------------
// Anonymous-namespace helpers for handle resolution
// ---------------------------------------------------------------------------

namespace {

  /// Stage and access flags for a given image layout transition endpoint.
  struct BarrierStageAccess {
    /// Pipeline stage mask for the barrier.
    VkPipelineStageFlags2 stage = VK_PIPELINE_STAGE_2_NONE;
    /// Access mask for the barrier.
    VkAccessFlags2 access = VK_ACCESS_2_NONE;
  };

  // Named algorithm: stageMaskForLayout
  // Stateless 1:1 mapping from RhiTextureLayout to Vulkan pipeline stage and
  // access flags. No side effects; pure lookup table.
  BarrierStageAccess stageMaskForLayout(RhiTextureLayout layout) {
    switch (layout) {
      // NOLINTNEXTLINE(bugprone-branch-clone) — FP without SDK headers
      case RhiTextureLayout::RENDER_TARGET:
        return {VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
                VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT};
      case RhiTextureLayout::DEPTH_STENCIL:
        return {VK_PIPELINE_STAGE_2_EARLY_FRAGMENT_TESTS_BIT,
                VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT};
      case RhiTextureLayout::SHADER_READ_ONLY:
        return {VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT,
                VK_ACCESS_2_SHADER_READ_BIT};
      case RhiTextureLayout::TRANSFER_SRC:
        return {VK_PIPELINE_STAGE_2_TRANSFER_BIT,
                VK_ACCESS_2_TRANSFER_READ_BIT};
      case RhiTextureLayout::TRANSFER_DST:
        return {VK_PIPELINE_STAGE_2_TRANSFER_BIT,
                VK_ACCESS_2_TRANSFER_WRITE_BIT};
      case RhiTextureLayout::PRESENT:
        return {VK_PIPELINE_STAGE_2_BOTTOM_OF_PIPE_BIT, VK_ACCESS_2_NONE};
      case RhiTextureLayout::GENERAL:
        return {VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
                VK_ACCESS_2_SHADER_READ_BIT | VK_ACCESS_2_SHADER_WRITE_BIT};
      case RhiTextureLayout::UNDEFINED:
      default:
        return {VK_PIPELINE_STAGE_2_TOP_OF_PIPE_BIT, VK_ACCESS_2_NONE};
    }
  }

  VkImage resolveImage(VulkanDevice::Impl& impl, RhiTextureHandle handle) {
    auto sc_count = static_cast<RhiTextureHandle>(impl.swapchain_images.size());
    if (handle > 0 && handle <= sc_count) {
      return impl.swapchain_images[handle - 1];
    }
    auto* tex = impl.textures.lookup(handle);
    if (tex == nullptr) {
      return VK_NULL_HANDLE;
    }
    return tex->image;
  }

  VkImageView resolveImageView(VulkanDevice::Impl& impl,
                               RhiTextureHandle handle) {
    auto sc_count = static_cast<RhiTextureHandle>(impl.swapchain_views.size());
    if (handle > 0 && handle <= sc_count) {
      return impl.swapchain_views[handle - 1];
    }
    auto* tex = impl.textures.lookup(handle);
    if (tex == nullptr) {
      return VK_NULL_HANDLE;
    }
    return tex->view;
  }

  /// Copy RGBA clear color from the RHI info into a Vulkan clear value.
  void setClearColor(VkClearValue& clear, const float (&color)[4]) {
    clear.color.float32[0] = color[0];
    clear.color.float32[1] = color[1];
    clear.color.float32[2] = color[2];
    clear.color.float32[3] = color[3];
  }

  /// Build a color rendering attachment from RHI render pass info.
  VkRenderingAttachmentInfo
  buildColorAttachment(const RhiRenderPassBeginInfo& info,
                       VulkanDevice::Impl& impl) {
    VkRenderingAttachmentInfo att{};
    att.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;
    att.loadOp = toVkLoadOp(info.color_load_op);
    att.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    setClearColor(att.clearValue, info.clear_color);
    att.imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    if (info.color_target_count > 0 && info.color_targets != nullptr) {
      att.imageView = resolveImageView(impl, info.color_targets[0]);
    }
    return att;
  }

  /// Build the VkRenderingInfo referencing a color attachment.
  VkRenderingInfo buildRenderingInfo(const RhiRenderPassBeginInfo& /*info*/,
                                     VulkanDevice::Impl& impl,
                                     VkRenderingAttachmentInfo& color_att) {
    VkRenderingInfo ri{};
    ri.sType = VK_STRUCTURE_TYPE_RENDERING_INFO;
    ri.renderArea = {{0, 0}, impl.swapchain_extent};
    ri.layerCount = 1;
    ri.colorAttachmentCount = 1;
    ri.pColorAttachments = &color_att;
    return ri;
  }

  /// Resolve texture dimensions (user texture or swapchain fallback).
  VkExtent2D resolveTextureDimensions(VulkanDevice::Impl& impl,
                                      RhiTextureHandle handle) {
    auto* tex = impl.textures.lookup(handle);
    if (tex != nullptr) {
      return {tex->width, tex->height};
    }
    return impl.swapchain_extent;
  }

  // Named algorithm: buildImageBarrier2
  // Stateless builder that translates RHI layout enums to a fully populated
  // VkImageMemoryBarrier2 with synchronisation2 stage/access masks.
  // No side effects; pure struct construction.
  VkImageMemoryBarrier2 buildImageBarrier2(VkImage image,
                                           RhiTextureLayout old_layout,
                                           RhiTextureLayout new_layout) {
    auto src = stageMaskForLayout(old_layout);
    auto dst = stageMaskForLayout(new_layout);
    VkImageMemoryBarrier2 barrier{};
    barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2;
    barrier.srcStageMask = src.stage;
    barrier.srcAccessMask = src.access;
    barrier.dstStageMask = dst.stage;
    barrier.dstAccessMask = dst.access;
    barrier.oldLayout = toVkImageLayout(old_layout);
    barrier.newLayout = toVkImageLayout(new_layout);
    barrier.image = image;
    barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    barrier.subresourceRange.levelCount = 1;
    barrier.subresourceRange.layerCount = 1;
    return barrier;
  }

  /// Issue a pipeline barrier with a single image memory barrier.
  void issueBarrier2(VkCommandBuffer cmd, VkImageMemoryBarrier2& barrier) {
    VkDependencyInfo dep_info{};
    dep_info.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO;
    dep_info.imageMemoryBarrierCount = 1;
    dep_info.pImageMemoryBarriers = &barrier;
    vkCmdPipelineBarrier2(cmd, &dep_info);
  }

  /// Build a depth rendering attachment from RHI render pass info.
  VkRenderingAttachmentInfo
  buildDepthAttachment(const RhiRenderPassBeginInfo& info,
                       VulkanDevice::Impl& impl) {
    VkRenderingAttachmentInfo att{};
    att.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;
    att.imageView = resolveImageView(impl, info.depth_target);
    att.imageLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
    att.loadOp = toVkLoadOp(info.depth_load_op);
    att.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    att.clearValue.depthStencil = {info.clear_depth, info.clear_stencil};
    return att;
  }

}  // namespace

// ---------------------------------------------------------------------------
// Construction
// ---------------------------------------------------------------------------

VulkanCommandList::VulkanCommandList(VkCommandBuffer cmd_buffer,
                                     VulkanDevice::Impl& impl)
  : cmd_buffer_(cmd_buffer), impl_(impl) {}

// ---------------------------------------------------------------------------
// Recording lifecycle
// ---------------------------------------------------------------------------

void VulkanCommandList::begin() {
  VkCommandBufferBeginInfo begin_info{};
  begin_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
  begin_info.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
  vkBeginCommandBuffer(cmd_buffer_, &begin_info);
}

void VulkanCommandList::end() {
  vkEndCommandBuffer(cmd_buffer_);
}

// ---------------------------------------------------------------------------
// Render pass
// ---------------------------------------------------------------------------

void VulkanCommandList::beginRenderPass(const RhiRenderPassBeginInfo& info) {
  auto color_att = buildColorAttachment(info, impl_);
  auto rendering = buildRenderingInfo(info, impl_, color_att);
  VkRenderingAttachmentInfo depth_att{};
  if (info.depth_target != RHI_TEXTURE_INVALID) {
    depth_att = buildDepthAttachment(info, impl_);
    rendering.pDepthAttachment = &depth_att;
  }
  vkCmdBeginRendering(cmd_buffer_, &rendering);
}

void VulkanCommandList::endRenderPass() {
  vkCmdEndRendering(cmd_buffer_);
}

// ---------------------------------------------------------------------------
// Pipeline binding
// ---------------------------------------------------------------------------

void VulkanCommandList::bindPipeline(RhiPipelineHandle pipeline) {
  auto* p = impl_.pipelines.lookup(pipeline);
  if (p == nullptr) {
    return;
  }
  vkCmdBindPipeline(cmd_buffer_, p->bind_point, p->pipeline);
}

// ---------------------------------------------------------------------------
// Resource binding
// ---------------------------------------------------------------------------

void VulkanCommandList::bindVertexBuffer(RhiBufferHandle buffer,
                                         uint64_t offset) {
  auto* buf = impl_.buffers.lookup(buffer);
  if (buf == nullptr) {
    return;
  }
  VkDeviceSize vk_offset = offset;
  vkCmdBindVertexBuffers(cmd_buffer_, 0, 1, &buf->buffer, &vk_offset);
}

void VulkanCommandList::bindIndexBuffer(RhiBufferHandle buffer, uint64_t offset,
                                        RhiIndexType index_type) {
  auto* buf = impl_.buffers.lookup(buffer);
  if (buf == nullptr) {
    return;
  }
  vkCmdBindIndexBuffer(cmd_buffer_, buf->buffer, offset,
                       toVkIndexType(index_type));
}

void VulkanCommandList::bindDescriptorSet(uint32_t /*set_index*/,
                                          RhiDescriptorSetHandle /*set*/) {
  // Deferred to render-pipeline Phase 1.
}

void VulkanCommandList::setVertexStageBytes(const void* data, size_t size,
                                            uint32_t /*slot*/) {
  if (data == nullptr || size == 0) {
    return;
  }
  // Vulkan equivalent: push constants. Requires a pipeline layout with a push
  // constant range. Deferred to render-pipeline Phase 1 when pipeline layouts
  // are fully designed. For now, this is a no-op matching the base default.
}

void VulkanCommandList::bindFragmentTexture(RhiTextureHandle /*texture*/,
                                            uint32_t /*slot*/) {
  // Vulkan binds textures via descriptor sets, not per-slot like Metal.
  // Deferred to render-pipeline Phase 1 when descriptor set management is
  // designed. For now, this is a no-op matching the base default.
}

// ---------------------------------------------------------------------------
// Viewport and scissor
// ---------------------------------------------------------------------------

void VulkanCommandList::setViewport(const RhiViewport& viewport) {
  VkViewport vk_viewport{};
  vk_viewport.x = viewport.x;
  vk_viewport.y = viewport.y;
  vk_viewport.width = viewport.width;
  vk_viewport.height = viewport.height;
  vk_viewport.minDepth = viewport.min_depth;
  vk_viewport.maxDepth = viewport.max_depth;
  vkCmdSetViewport(cmd_buffer_, 0, 1, &vk_viewport);
}

void VulkanCommandList::setScissor(const RhiScissor& scissor) {
  VkRect2D vk_scissor{};
  vk_scissor.offset.x = scissor.x;
  vk_scissor.offset.y = scissor.y;
  vk_scissor.extent.width = scissor.width;
  vk_scissor.extent.height = scissor.height;
  vkCmdSetScissor(cmd_buffer_, 0, 1, &vk_scissor);
}

// ---------------------------------------------------------------------------
// Draw commands
// ---------------------------------------------------------------------------

void VulkanCommandList::draw(const RhiDrawParams& params) {
  vkCmdDraw(cmd_buffer_, params.vertex_count, params.instance_count,
            params.first_vertex, params.first_instance);
}

void VulkanCommandList::drawIndexed(const RhiDrawIndexedParams& params) {
  vkCmdDrawIndexed(cmd_buffer_, params.index_count, params.instance_count,
                   params.first_index, params.vertex_offset,
                   params.first_instance);
}

// ---------------------------------------------------------------------------
// Compute dispatch
// ---------------------------------------------------------------------------

void VulkanCommandList::dispatch(uint32_t groups_x, uint32_t groups_y,
                                 uint32_t groups_z) {
  vkCmdDispatch(cmd_buffer_, groups_x, groups_y, groups_z);
}

// ---------------------------------------------------------------------------
// Copy commands
// ---------------------------------------------------------------------------

void VulkanCommandList::copyBuffer(const RhiCopyBufferParams& params) {
  auto* src = impl_.buffers.lookup(params.src);
  auto* dst = impl_.buffers.lookup(params.dst);
  if (src == nullptr || dst == nullptr) {
    return;
  }
  VkBufferCopy region{};
  region.srcOffset = params.src_offset;
  region.dstOffset = params.dst_offset;
  region.size = params.size;
  vkCmdCopyBuffer(cmd_buffer_, src->buffer, dst->buffer, 1, &region);
}

void VulkanCommandList::copyTextureToBuffer(RhiTextureHandle src,
                                            RhiBufferHandle dst) {
  VkImage image = resolveImage(impl_, src);
  auto* buf = impl_.buffers.lookup(dst);
  if (image == VK_NULL_HANDLE || buf == nullptr) {
    return;
  }
  auto dims = resolveTextureDimensions(impl_, src);
  VkBufferImageCopy region{};
  region.imageSubresource = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1};
  region.imageExtent = {dims.width, dims.height, 1};
  vkCmdCopyImageToBuffer(cmd_buffer_, image,
                         VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, buf->buffer, 1,
                         &region);
}

// ---------------------------------------------------------------------------
// Compute texture binding
// ---------------------------------------------------------------------------

void VulkanCommandList::bindComputeStorageImage(RhiTextureHandle /*texture*/,
                                                uint32_t /*mip_level*/,
                                                uint32_t /*slot*/) {
  // Vulkan binds storage images via descriptor sets. Implementation requires
  // per-mip image views and descriptor set updates. Deferred to descriptor
  // set management integration.
}

void VulkanCommandList::bindComputeSampledTexture(RhiTextureHandle /*texture*/,
                                                  uint32_t /*slot*/) {
  // Vulkan binds sampled textures via descriptor sets. Deferred to descriptor
  // set management integration.
}

// ---------------------------------------------------------------------------
// Barriers
// ---------------------------------------------------------------------------

void VulkanCommandList::computeBarrier() {
  VkMemoryBarrier2 mem_barrier{};
  mem_barrier.sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER_2;
  mem_barrier.srcStageMask = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT;
  mem_barrier.srcAccessMask = VK_ACCESS_2_SHADER_WRITE_BIT;
  mem_barrier.dstStageMask = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT;
  mem_barrier.dstAccessMask = VK_ACCESS_2_SHADER_READ_BIT;
  VkDependencyInfo dep_info{};
  dep_info.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO;
  dep_info.memoryBarrierCount = 1;
  dep_info.pMemoryBarriers = &mem_barrier;
  vkCmdPipelineBarrier2(cmd_buffer_, &dep_info);
}

void VulkanCommandList::textureBarrier(RhiTextureHandle texture,
                                       RhiTextureLayout old_layout,
                                       RhiTextureLayout new_layout) {
  VkImage image = resolveImage(impl_, texture);
  auto barrier = buildImageBarrier2(image, old_layout, new_layout);
  issueBarrier2(cmd_buffer_, barrier);
}

// ---------------------------------------------------------------------------
// Native access
// ---------------------------------------------------------------------------

VkCommandBuffer VulkanCommandList::nativeCommandBuffer() const {
  return cmd_buffer_;
}

}  // namespace eng::render

#endif  // ENGINE_RENDERER_VULKAN
