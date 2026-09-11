#include "vulkan-command-list.h"

#ifdef ENGINE_RENDERER_VULKAN

#include "vulkan-device-impl.h"
#include "vulkan-format-map.h"
#include "vulkan-image-transition.h"

#include <algorithm>
#include <array>
#include <cstdint>
#include <engine/render/rhi-types.h>
#include <vulkan/vulkan.h>

namespace eng::render {

// ---------------------------------------------------------------------------
// Anonymous-namespace helpers
// ---------------------------------------------------------------------------

namespace {

  /// Stage and access flags for one side of a buffer barrier.
  struct BarrierStageAccess {
    /// Pipeline stage mask for the barrier.
    VkPipelineStageFlags2 stage = VK_PIPELINE_STAGE_2_NONE;
    /// Access mask for the barrier.
    VkAccessFlags2 access = VK_ACCESS_2_NONE;
  };

  /// One `RhiBarrierStage` bit and the stage and access it stands for.
  struct BarrierStageEntry {
    /// The RHI bit.
    RhiBarrierStage bit = RhiBarrierStage::COMPUTE_WRITE;
    /// What it maps to.
    BarrierStageAccess sync{};
  };

  constexpr std::array<BarrierStageEntry, 7> BARRIER_STAGES{{
      {RhiBarrierStage::COMPUTE_WRITE,
       {VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
        VK_ACCESS_2_SHADER_STORAGE_WRITE_BIT}},
      {RhiBarrierStage::INDIRECT_READ,
       {VK_PIPELINE_STAGE_2_DRAW_INDIRECT_BIT,
        VK_ACCESS_2_INDIRECT_COMMAND_READ_BIT}},
      {RhiBarrierStage::VERTEX_READ,
       {VK_PIPELINE_STAGE_2_VERTEX_ATTRIBUTE_INPUT_BIT,
        VK_ACCESS_2_VERTEX_ATTRIBUTE_READ_BIT}},
      {RhiBarrierStage::INDEX_READ,
       {VK_PIPELINE_STAGE_2_INDEX_INPUT_BIT, VK_ACCESS_2_INDEX_READ_BIT}},
      {RhiBarrierStage::TRANSFER_WRITE,
       {VK_PIPELINE_STAGE_2_COPY_BIT, VK_ACCESS_2_TRANSFER_WRITE_BIT}},
      {RhiBarrierStage::TRANSFER_READ,
       {VK_PIPELINE_STAGE_2_COPY_BIT, VK_ACCESS_2_TRANSFER_READ_BIT}},
      {RhiBarrierStage::FRAGMENT_READ,
       {VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT, VK_ACCESS_2_SHADER_READ_BIT}},
  }};

  /// Every stage and access the set bits of `stages` name.
  BarrierStageAccess barrierSync(RhiBarrierStage stages) {
    BarrierStageAccess sync{};
    for (const BarrierStageEntry& entry : BARRIER_STAGES) {
      if (stages & entry.bit) {
        sync.stage |= entry.sync.stage;
        sync.access |= entry.sync.access;
      }
    }
    return sync;
  }

  VkBufferMemoryBarrier2 buildBufferBarrier(VkBuffer buffer,
                                            RhiBarrierStage src_stage,
                                            RhiBarrierStage dst_stage) {
    const BarrierStageAccess src = barrierSync(src_stage);
    const BarrierStageAccess dst = barrierSync(dst_stage);
    VkBufferMemoryBarrier2 barrier{};
    barrier.sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER_2;
    barrier.srcStageMask = src.stage;
    barrier.srcAccessMask = src.access;
    barrier.dstStageMask = dst.stage;
    barrier.dstAccessMask = dst.access;
    barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.buffer = buffer;
    barrier.size = VK_WHOLE_SIZE;
    return barrier;
  }

  /// Copy RGBA clear color from the RHI info into a Vulkan clear value.
  void setClearColor(VkClearValue& clear, const float (&color)[4]) {
    clear.color.float32[0] = color[0];
    clear.color.float32[1] = color[1];
    clear.color.float32[2] = color[2];
    clear.color.float32[3] = color[3];
  }

  VkRenderingAttachmentInfo colorAttachment(const RhiRenderPassBeginInfo& info,
                                            VkImageView view) {
    VkRenderingAttachmentInfo att{};
    att.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;
    att.imageView = view;
    att.imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    att.loadOp = toVkLoadOp(info.color_load_op);
    att.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    setClearColor(att.clearValue, info.clear_color);
    return att;
  }

  VkRenderingAttachmentInfo depthAttachment(const RhiRenderPassBeginInfo& info,
                                            VkImageView view) {
    VkRenderingAttachmentInfo att{};
    att.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;
    att.imageView = view;
    att.imageLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
    att.loadOp = toVkLoadOp(info.depth_load_op);
    att.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    att.clearValue.depthStencil = {info.clear_depth, info.clear_stencil};
    return att;
  }

  VkRenderingInfo buildRenderingInfo(const VulkanPassAttachments& pass) {
    VkRenderingInfo ri{};
    ri.sType = VK_STRUCTURE_TYPE_RENDERING_INFO;
    ri.renderArea = {{0, 0}, pass.extent};
    ri.layerCount = 1;
    ri.colorAttachmentCount = pass.color_count;
    ri.pColorAttachments = pass.colors.data();
    if (pass.depth_target != RHI_TEXTURE_INVALID) {
      ri.pDepthAttachment = &pass.depth;
    }
    return ri;
  }

  VkWriteDescriptorSet uniformWrite(uint32_t binding,
                                    const VkDescriptorBufferInfo* range) {
    VkWriteDescriptorSet write{};
    write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    write.dstBinding = binding;
    write.descriptorCount = 1;
    write.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    write.pBufferInfo = range;
    return write;
  }

  VkWriteDescriptorSet imageWrite(const VkDescriptorImageInfo* image) {
    VkWriteDescriptorSet write{};
    write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    write.dstBinding = VULKAN_BINDING_FRAGMENT_TEXTURE;
    write.descriptorCount = 1;
    write.descriptorType = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
    write.pImageInfo = image;
    return write;
  }

  /// The writes of one push and the infos they point into, kept together
  /// so the pointers stay valid until the push is recorded.
  struct PushedSet {
    /// Range written at each uniform binding.
    std::array<VkDescriptorBufferInfo, VULKAN_UNIFORM_BINDING_COUNT> ranges{};
    /// Image written at the texture binding.
    VkDescriptorImageInfo image{};
    /// One write per pushed binding, pointing into the two above.
    std::array<VkWriteDescriptorSet, VULKAN_PUSHED_BINDING_COUNT> writes{};
  };

  /// Fill `out` from `b`, with `empty` for any uniform slot never set and
  /// `fallback` for an unset texture. Every binding a shader may read is
  /// written, which Vulkan requires and Metal never asked for.
  void fillPushedSet(const VulkanStageBindings& b,
                     const VkDescriptorBufferInfo& empty, VkImageView fallback,
                     PushedSet& out) {
    for (uint32_t i = 0; i < VULKAN_UNIFORM_BINDING_COUNT; ++i) {
      const auto& set = b.uniforms[i];
      out.ranges[i] = set.buffer != VK_NULL_HANDLE ? set : empty;
      out.writes[i] = uniformWrite(i, &out.ranges[i]);
    }
    out.image = {VK_NULL_HANDLE,
                 b.texture != VK_NULL_HANDLE ? b.texture : fallback,
                 VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL};
    out.writes[VULKAN_BINDING_FRAGMENT_TEXTURE] = imageWrite(&out.image);
  }

  /// `stride` as given, or a tightly packed one when the caller left it 0.
  uint32_t indirectStride(uint32_t stride, uint32_t packed) {
    return stride != 0 ? stride : packed;
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
  graphics_bound_ = false;
  in_pass_ = false;
  bindings_ = {};
  pass_ = {};
}

void VulkanCommandList::end() {
  endRenderPass();
  // The acquired image goes to the presentation engine in PRESENT, whether
  // or not a pass drew into it.
  const auto backbuffer = static_cast<RhiTextureHandle>(impl_.image_index) + 1;
  transitionVulkanImage(cmd_buffer_, impl_.imageRef(backbuffer),
                        VK_IMAGE_LAYOUT_PRESENT_SRC_KHR);
  vkEndCommandBuffer(cmd_buffer_);
}

// ---------------------------------------------------------------------------
// Render pass
// ---------------------------------------------------------------------------

void VulkanCommandList::attachColors(const RhiRenderPassBeginInfo& info) {
  const uint32_t requested =
      info.color_targets != nullptr
          ? std::min(info.color_target_count, VULKAN_MAX_COLOR_TARGETS)
          : 0;
  for (uint32_t i = 0; i < requested; ++i) {
    const VulkanImageRef ref = impl_.imageRef(info.color_targets[i]);
    if (ref.view == VK_NULL_HANDLE) {
      break;  // A target with no view ends the run, as it does on DX12.
    }
    transitionVulkanImage(cmd_buffer_, ref,
                          VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
    pass_.extent = pass_.color_count == 0 ? ref.extent : pass_.extent;
    pass_.colors[pass_.color_count] = colorAttachment(info, ref.view);
    pass_.color_targets[pass_.color_count] = info.color_targets[i];
    ++pass_.color_count;
  }
}

void VulkanCommandList::attachDepth(const RhiRenderPassBeginInfo& info) {
  const VulkanImageRef ref = impl_.imageRef(info.depth_target);
  if (ref.view == VK_NULL_HANDLE) {
    return;
  }
  transitionVulkanImage(cmd_buffer_, ref,
                        VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL);
  pass_.depth = depthAttachment(info, ref.view);
  pass_.depth_target = info.depth_target;
  pass_.extent = pass_.color_count == 0 ? ref.extent : pass_.extent;
}

void VulkanCommandList::beginRenderPass(const RhiRenderPassBeginInfo& info) {
  // Metal would refuse a second encoder; closing the first is the gentler
  // reading of the same mistake.
  endRenderPass();
  pass_ = {};
  attachColors(info);
  attachDepth(info);
  if (pass_.extent.width == 0 || pass_.extent.height == 0) {
    return;
  }
  const VkRenderingInfo rendering = buildRenderingInfo(pass_);
  vkCmdBeginRendering(cmd_buffer_, &rendering);
  in_pass_ = true;
  bindings_.dirty = true;
  setFullViewport(pass_.extent);
}

void VulkanCommandList::settleTarget(RhiTextureHandle texture) {
  const VulkanImageRef ref = impl_.imageRef(texture);
  if (ref.layout != nullptr) {
    transitionVulkanImage(cmd_buffer_, ref, vulkanRestingLayout(ref));
  }
}

void VulkanCommandList::endRenderPass() {
  if (!in_pass_) {
    return;
  }
  vkCmdEndRendering(cmd_buffer_);
  in_pass_ = false;
  // Sampled targets go back to the shader-read layout, so the next pass
  // can bind them: the outline reads the depth the scene pass just wrote.
  for (uint32_t i = 0; i < pass_.color_count; ++i) {
    settleTarget(pass_.color_targets[i]);
  }
  settleTarget(pass_.depth_target);
  pass_ = {};
}

void VulkanCommandList::setFullViewport(VkExtent2D extent) {
  RhiViewport viewport{};
  viewport.width = static_cast<float>(extent.width);
  viewport.height = static_cast<float>(extent.height);
  setViewport(viewport);
  RhiScissor scissor{};
  scissor.width = extent.width;
  scissor.height = extent.height;
  setScissor(scissor);
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
  if (p->bind_point == VK_PIPELINE_BIND_POINT_GRAPHICS) {
    graphics_bound_ = true;
    bindings_.dirty = true;
  }
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
  // The RHI has no descriptor sets yet; see vulkan-shared-layout.h.
}

void VulkanCommandList::stageBytes(uint32_t binding, const void* data,
                                   size_t size) {
  if (binding == VULKAN_BINDING_NONE || data == nullptr || size == 0) {
    return;
  }
  auto& ring = impl_.frames[impl_.frame_index].stage_bytes;
  const VkDescriptorBufferInfo range = ring.push(data, size);
  if (range.buffer == VK_NULL_HANDLE) {
    return;
  }
  bindings_.uniforms[binding] = range;
  bindings_.dirty = true;
}

void VulkanCommandList::setVertexStageBytes(const void* data, size_t size,
                                            uint32_t slot) {
  stageBytes(vulkanVertexUniformBinding(slot), data, size);
}

void VulkanCommandList::setFragmentStageBytes(const void* data, size_t size,
                                              uint32_t slot) {
  stageBytes(vulkanFragmentUniformBinding(slot), data, size);
}

VkImageView VulkanCommandList::sampleableView(const VulkanImageRef& ref) {
  if (ref.view == VK_NULL_HANDLE ||
      (ref.usage & VK_IMAGE_USAGE_SAMPLED_BIT) == 0) {
    return VK_NULL_HANDLE;
  }
  if (*ref.layout != VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL) {
    // No barrier can move it inside a pass; the stand-in is better than an
    // image in the wrong layout.
    if (in_pass_) {
      return VK_NULL_HANDLE;
    }
    transitionVulkanImage(cmd_buffer_, ref,
                          VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
  }
  return ref.view;
}

void VulkanCommandList::bindFragmentTexture(RhiTextureHandle texture,
                                            uint32_t slot) {
  if (slot != 0) {
    return;
  }
  bindings_.texture = sampleableView(impl_.imageRef(texture));
  bindings_.dirty = true;
}

void VulkanCommandList::flushBindings() {
  if (!bindings_.dirty) {
    return;
  }
  PushedSet set;
  fillPushedSet(bindings_, {impl_.null_uniform.buffer, 0, VK_WHOLE_SIZE},
                impl_.imageRef(impl_.stand_in_texture).view, set);
  impl_.push_descriptor_set(cmd_buffer_, VK_PIPELINE_BIND_POINT_GRAPHICS,
                            impl_.shared_layout.graphics, 0,
                            static_cast<uint32_t>(set.writes.size()),
                            set.writes.data());
  bindings_.dirty = false;
}

// ---------------------------------------------------------------------------
// Viewport and scissor
// ---------------------------------------------------------------------------

void VulkanCommandList::setViewport(const RhiViewport& viewport) {
  // Metal and D3D put +Y up in clip space, Vulkan puts it down. A viewport
  // of negative height (core since 1.1) turns it back, so one projection
  // comes out the same way up on every backend.
  VkViewport vk_viewport{};
  vk_viewport.x = viewport.x;
  vk_viewport.y = viewport.y + viewport.height;
  vk_viewport.width = viewport.width;
  vk_viewport.height = -viewport.height;
  vk_viewport.minDepth = viewport.min_depth;
  vk_viewport.maxDepth = viewport.max_depth;
  vkCmdSetViewport(cmd_buffer_, 0, 1, &vk_viewport);
}

void VulkanCommandList::setScissor(const RhiScissor& scissor) {
  // Vulkan rejects a negative offset; Metal's is unsigned to begin with.
  VkRect2D vk_scissor{};
  vk_scissor.offset.x = std::max(scissor.x, 0);
  vk_scissor.offset.y = std::max(scissor.y, 0);
  vk_scissor.extent.width = scissor.width;
  vk_scissor.extent.height = scissor.height;
  vkCmdSetScissor(cmd_buffer_, 0, 1, &vk_scissor);
}

// ---------------------------------------------------------------------------
// Draw commands
// ---------------------------------------------------------------------------

bool VulkanCommandList::prepareDraw() {
  if (!in_pass_ || !graphics_bound_) {
    return false;
  }
  flushBindings();
  return true;
}

void VulkanCommandList::draw(const RhiDrawParams& params) {
  if (!prepareDraw()) {
    return;
  }
  vkCmdDraw(cmd_buffer_, params.vertex_count, params.instance_count,
            params.first_vertex, params.first_instance);
}

void VulkanCommandList::drawIndexed(const RhiDrawIndexedParams& params) {
  if (!prepareDraw()) {
    return;
  }
  vkCmdDrawIndexed(cmd_buffer_, params.index_count, params.instance_count,
                   params.first_index, params.vertex_offset,
                   params.first_instance);
}

void VulkanCommandList::drawIndirect(const RhiDrawIndirectParams& params) {
  auto* buf = impl_.buffers.lookup(params.buffer);
  if (buf == nullptr || !prepareDraw()) {
    return;
  }
  vkCmdDrawIndirect(
      cmd_buffer_, buf->buffer, params.offset, params.draw_count,
      indirectStride(params.stride, sizeof(VkDrawIndirectCommand)));
}

void VulkanCommandList::drawIndexedIndirect(
    const RhiDrawIndexedIndirectParams& params) {
  auto* buf = impl_.buffers.lookup(params.buffer);
  if (buf == nullptr || !prepareDraw()) {
    return;
  }
  vkCmdDrawIndexedIndirect(
      cmd_buffer_, buf->buffer, params.offset, params.draw_count,
      indirectStride(params.stride, sizeof(VkDrawIndexedIndirectCommand)));
}

void VulkanCommandList::drawIndexedIndirectCount(
    const RhiDrawIndexedIndirectCountParams& params) {
  auto* args = impl_.buffers.lookup(params.arg_buffer);
  auto* count = impl_.buffers.lookup(params.count_buffer);
  // Without the feature (MoltenVK has none) this is the base no-op.
  if (args == nullptr || count == nullptr || !impl_.draw_indirect_count ||
      !prepareDraw()) {
    return;
  }
  vkCmdDrawIndexedIndirectCount(
      cmd_buffer_, args->buffer, params.arg_offset, count->buffer,
      params.count_offset, params.max_draw_count,
      indirectStride(params.stride, sizeof(VkDrawIndexedIndirectCommand)));
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
  const VulkanImageRef ref = impl_.imageRef(src);
  auto* buf = impl_.buffers.lookup(dst);
  if (ref.image == VK_NULL_HANDLE || buf == nullptr || in_pass_) {
    return;
  }
  const VkImageLayout before = *ref.layout;
  transitionVulkanImage(cmd_buffer_, ref, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL);
  VkBufferImageCopy region{};
  region.imageSubresource = {ref.aspect, 0, 0, 1};
  region.imageExtent = {ref.extent.width, ref.extent.height, 1};
  vkCmdCopyImageToBuffer(cmd_buffer_, ref.image,
                         VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, buf->buffer, 1,
                         &region);
  if (before != VK_IMAGE_LAYOUT_UNDEFINED) {
    transitionVulkanImage(cmd_buffer_, ref, before);
  }
}

// ---------------------------------------------------------------------------
// Compute texture binding
// ---------------------------------------------------------------------------

void VulkanCommandList::bindComputeStorageImage(RhiTextureHandle /*texture*/,
                                                uint32_t /*mip_level*/,
                                                uint32_t /*slot*/) {
  // Vulkan binds storage images via descriptor sets. Implementation requires
  // per-mip image views and descriptor set updates. Deferred to descriptor
  // set management integration; the Metal backend has no binding here either.
}

void VulkanCommandList::bindComputeSampledTexture(RhiTextureHandle /*texture*/,
                                                  uint32_t /*slot*/) {
  // Vulkan binds sampled textures via descriptor sets. Deferred to descriptor
  // set management integration; the Metal backend has no binding here either.
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

void VulkanCommandList::bufferBarrier(RhiBufferHandle buffer,
                                      RhiBarrierStage src_stage,
                                      RhiBarrierStage dst_stage) {
  auto* buf = impl_.buffers.lookup(buffer);
  if (buf == nullptr || in_pass_) {
    return;
  }
  const VkBufferMemoryBarrier2 barrier =
      buildBufferBarrier(buf->buffer, src_stage, dst_stage);
  VkDependencyInfo dep_info{};
  dep_info.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO;
  dep_info.bufferMemoryBarrierCount = 1;
  dep_info.pBufferMemoryBarriers = &barrier;
  vkCmdPipelineBarrier2(cmd_buffer_, &dep_info);
}

void VulkanCommandList::textureBarrier(RhiTextureHandle texture,
                                       RhiTextureLayout /*old_layout*/,
                                       RhiTextureLayout new_layout) {
  // The backend knows which layout the image is really in, so only where
  // the caller wants it matters.
  const VkImageLayout to = toVkImageLayout(new_layout);
  if (in_pass_ || to == VK_IMAGE_LAYOUT_UNDEFINED) {
    return;
  }
  transitionVulkanImage(cmd_buffer_, impl_.imageRef(texture), to);
}

// ---------------------------------------------------------------------------
// Native access
// ---------------------------------------------------------------------------

VkCommandBuffer VulkanCommandList::nativeCommandBuffer() const {
  return cmd_buffer_;
}

}  // namespace eng::render

#endif  // ENGINE_RENDERER_VULKAN
