#include "vulkan-command-list.h"
#include "vulkan-device-impl.h"
#include "vulkan-format-map.h"

#ifdef ENGINE_RENDERER_VULKAN

#include <array>
#include <cstdint>
#include <cstring>
#include <engine/render/rhi-capture-request.h>
#include <engine/render/rhi-capture-result.h>
#include <engine/render/rhi-command-list.h>
#include <engine/render/rhi-texture-update-2d.h>
#include <engine/render/rhi-types.h>
#include <fstream>
#include <memory>
#include <optional>
#include <string_view>
#include <vector>
#include <vk_mem_alloc.h>
#include <vulkan/vulkan.h>

// stb_image_write — PNG/JPEG encoding for capture API.
// Implementation lives in stb-image-write-impl.cpp.
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wold-style-cast"
#pragma clang diagnostic ignored "-Wsign-conversion"
#include <stb_image_write.h>
#pragma clang diagnostic pop

namespace eng::render {

// ---------------------------------------------------------------------------
// Anonymous-namespace helpers for resource creation decomposition
// ---------------------------------------------------------------------------

namespace {

  VkBufferCreateInfo buildBufferCreateInfo(const RhiBufferDesc& desc) {
    VkBufferCreateInfo info{};
    info.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    info.size = desc.size;
    info.usage = toVkBufferUsage(desc.usage);
    return info;
  }

  /// Whether a buffer should be host-visible (CPU-mappable).
  enum class HostVisible { NO, YES };

  VmaAllocationCreateInfo buildBufferAllocInfo(HostVisible host_visible) {
    VmaAllocationCreateInfo info{};
    info.usage = host_visible == HostVisible::YES ? VMA_MEMORY_USAGE_CPU_TO_GPU
                                                  : VMA_MEMORY_USAGE_GPU_ONLY;
    return info;
  }

  VkImageCreateInfo buildImageCreateInfo(const RhiTextureDesc& desc) {
    VkImageCreateInfo info{};
    info.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    info.imageType = VK_IMAGE_TYPE_2D;
    info.format = toVkFormat(desc.format);
    info.extent = {desc.width, desc.height, desc.depth};
    info.mipLevels = desc.mip_levels;
    info.arrayLayers = desc.array_layers;
    info.samples = VK_SAMPLE_COUNT_1_BIT;
    info.tiling = VK_IMAGE_TILING_OPTIMAL;
    info.usage = toVkImageUsage(desc.usage);
    return info;
  }

  /// Allocate a VMA image and populate a VulkanTexture from a descriptor.
  VulkanTexture allocateImage(VmaAllocator alloc,
                              const VkImageCreateInfo& img_info,
                              const RhiTextureDesc& desc) {
    VmaAllocationCreateInfo alloc_info{};
    alloc_info.usage = VMA_MEMORY_USAGE_GPU_ONLY;
    VulkanTexture vk_tex{};
    vk_tex.width = desc.width;
    vk_tex.height = desc.height;
    vk_tex.format = img_info.format;
    vk_tex.rhi_format = desc.format;
    vmaCreateImage(alloc, &img_info, &alloc_info, &vk_tex.image,
                   &vk_tex.allocation, nullptr);
    return vk_tex;
  }

  VkSubmitInfo buildSubmitInfo(const VkCommandBuffer* cmd,
                               const VkSemaphore* wait,
                               const VkPipelineStageFlags* wait_stage,
                               const VkSemaphore* signal) {
    VkSubmitInfo info{};
    info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    info.waitSemaphoreCount = 1;
    info.pWaitSemaphores = wait;
    info.pWaitDstStageMask = wait_stage;
    info.commandBufferCount = 1;
    info.pCommandBuffers = cmd;
    info.signalSemaphoreCount = 1;
    info.pSignalSemaphores = signal;
    return info;
  }

  VkPresentInfoKHR buildPresentInfo(const VkSemaphore* wait,
                                    const VkSwapchainKHR* swapchain,
                                    const uint32_t* image_index) {
    VkPresentInfoKHR info{};
    info.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
    info.waitSemaphoreCount = 1;
    info.pWaitSemaphores = wait;
    info.swapchainCount = 1;
    info.pSwapchains = swapchain;
    info.pImageIndices = image_index;
    return info;
  }

  // -----------------------------------------------------------------
  // Capture helpers
  // -----------------------------------------------------------------

  constexpr int CAPTURE_CHANNELS = 4;

  /// stb callback: append bytes to a vector.
  void stbWriteCallback(void* context, void* data, int size) {
    auto* out = static_cast<std::vector<uint8_t>*>(context);
    const auto* bytes = static_cast<const uint8_t*>(data);
    out->insert(out->end(), bytes, bytes + size);
  }

  /// Create a host-visible staging buffer for pixel readback.
  VulkanBuffer createCaptureStagingBuffer(VmaAllocator alloc,
                                          VkDeviceSize size) {
    VkBufferCreateInfo buf_info{};
    buf_info.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    buf_info.size = size;
    buf_info.usage = VK_BUFFER_USAGE_TRANSFER_DST_BIT;

    VmaAllocationCreateInfo alloc_info{};
    alloc_info.usage = VMA_MEMORY_USAGE_GPU_TO_CPU;

    VulkanBuffer buf{};
    buf.host_visible = true;
    vmaCreateBuffer(alloc, &buf_info, &alloc_info, &buf.buffer, &buf.allocation,
                    nullptr);
    return buf;
  }

  /// Encode raw RGBA pixels to PNG or JPEG via stb_image_write.
  std::vector<uint8_t> encodePixels(const uint8_t* data, uint32_t w, uint32_t h,
                                    const RhiCaptureRequest& req) {
    auto stride = static_cast<int>(w) * CAPTURE_CHANNELS;
    std::vector<uint8_t> encoded;

    if (req.format == RhiCaptureFormat::JPEG) {
      stbi_write_jpg_to_func(stbWriteCallback, &encoded, static_cast<int>(w),
                             static_cast<int>(h), CAPTURE_CHANNELS, data,
                             static_cast<int>(req.jpeg_quality));
    } else {
      stbi_write_png_to_func(stbWriteCallback, &encoded, static_cast<int>(w),
                             static_cast<int>(h), CAPTURE_CHANNELS, data,
                             stride);
    }
    return encoded;
  }

  /// Parameters for building an image memory barrier.
  struct ImageBarrierParams {
    /// The image to transition.
    VkImage image = VK_NULL_HANDLE;
    /// Layout before the barrier.
    VkImageLayout old_layout = VK_IMAGE_LAYOUT_UNDEFINED;
    /// Layout after the barrier.
    VkImageLayout new_layout = VK_IMAGE_LAYOUT_UNDEFINED;
    /// Source access mask.
    VkAccessFlags src_access = 0;
    /// Destination access mask.
    VkAccessFlags dst_access = 0;
  };

  /// Build a pipeline barrier for image layout transitions.
  VkImageMemoryBarrier buildImageBarrier(const ImageBarrierParams& p) {
    VkImageMemoryBarrier barrier{};
    barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    barrier.oldLayout = p.old_layout;
    barrier.newLayout = p.new_layout;
    barrier.srcAccessMask = p.src_access;
    barrier.dstAccessMask = p.dst_access;
    barrier.image = p.image;
    barrier.subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};
    return barrier;
  }

  /// Transition swapchain image from present to transfer source.
  void barrierPresentToTransferSrc(VkCommandBuffer cmd, VkImage image) {
    auto barrier = buildImageBarrier({image, VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
                                      VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                                      VK_ACCESS_MEMORY_READ_BIT,
                                      VK_ACCESS_TRANSFER_READ_BIT});
    vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_TRANSFER_BIT,
                         VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, nullptr, 0,
                         nullptr, 1, &barrier);
  }

  /// Transition swapchain image from transfer source back to present.
  void barrierTransferSrcToPresent(VkCommandBuffer cmd, VkImage image) {
    auto barrier = buildImageBarrier(
        {image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
         VK_IMAGE_LAYOUT_PRESENT_SRC_KHR, VK_ACCESS_TRANSFER_READ_BIT,
         VK_ACCESS_MEMORY_READ_BIT});
    vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_TRANSFER_BIT,
                         VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, 0, 0, nullptr, 0,
                         nullptr, 1, &barrier);
  }

  /// Parameters for recording an image-to-buffer copy.
  struct CopyImageToBufferParams {
    /// Command buffer to record into.
    VkCommandBuffer cmd = VK_NULL_HANDLE;
    /// Source image to copy from.
    VkImage src_image = VK_NULL_HANDLE;
    /// Destination buffer to copy into.
    VkBuffer dst_buffer = VK_NULL_HANDLE;
    /// Width in pixels.
    uint32_t width = 0;
    /// Height in pixels.
    uint32_t height = 0;
  };

  /// Record copy from swapchain image to staging buffer.
  void recordCopyImageToBuffer(const CopyImageToBufferParams& p) {
    VkBufferImageCopy region{};
    region.imageSubresource = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1};
    region.imageExtent = {p.width, p.height, 1};
    vkCmdCopyImageToBuffer(p.cmd, p.src_image,
                           VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, p.dst_buffer,
                           1, &region);
  }

  /// Record full capture command sequence: barrier, copy, barrier.
  void recordCaptureCommands(const CopyImageToBufferParams& p) {
    barrierPresentToTransferSrc(p.cmd, p.src_image);
    recordCopyImageToBuffer(p);
    barrierTransferSrcToPresent(p.cmd, p.src_image);
  }

  /// Allocate and begin a one-shot command buffer.
  VkCommandBuffer beginOneShotCommandBuffer(VkDevice device,
                                            VkCommandPool pool) {
    VkCommandBufferAllocateInfo alloc_info{};
    alloc_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    alloc_info.commandPool = pool;
    alloc_info.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    alloc_info.commandBufferCount = 1;

    VkCommandBuffer cmd = VK_NULL_HANDLE;
    vkAllocateCommandBuffers(device, &alloc_info, &cmd);

    VkCommandBufferBeginInfo begin_info{};
    begin_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    begin_info.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    vkBeginCommandBuffer(cmd, &begin_info);
    return cmd;
  }

  /// Submit a command buffer and wait for completion.
  void submitAndWait(VkDevice device, VkQueue queue, VkCommandBuffer cmd) {
    vkEndCommandBuffer(cmd);

    VkSubmitInfo submit_info{};
    submit_info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submit_info.commandBufferCount = 1;
    submit_info.pCommandBuffers = &cmd;
    vkQueueSubmit(queue, 1, &submit_info, VK_NULL_HANDLE);
    vkQueueWaitIdle(queue);
  }

  /// Parameters for pixel readback and encoding.
  struct ReadbackParams {
    /// VMA allocator for map/unmap.
    VmaAllocator alloc = VK_NULL_HANDLE;
    /// Staging buffer holding pixel data.
    VulkanBuffer* staging = nullptr;
    /// Image width in pixels.
    uint32_t width = 0;
    /// Image height in pixels.
    uint32_t height = 0;
  };

  /// Map staging buffer, encode pixels, and unmap.
  std::vector<uint8_t> readbackAndEncode(const ReadbackParams& p,
                                         const RhiCaptureRequest& req) {
    void* mapped = nullptr;
    vmaMapMemory(p.alloc, p.staging->allocation, &mapped);
    auto encoded = encodePixels(static_cast<const uint8_t*>(mapped), p.width,
                                p.height, req);
    vmaUnmapMemory(p.alloc, p.staging->allocation);
    return encoded;
  }

  /// Parameters for a full capture operation.
  struct CaptureParams {
    /// Device implementation state.
    VulkanDevice::Impl* impl = nullptr;
    /// Source swapchain image.
    VkImage src_image = VK_NULL_HANDLE;
    /// Capture width in pixels.
    uint32_t width = 0;
    /// Capture height in pixels.
    uint32_t height = 0;
  };

  /// Record and submit capture commands to the GPU.
  void submitCaptureCommands(const CaptureParams& p, VkBuffer staging_buf) {
    auto cmd = beginOneShotCommandBuffer(p.impl->device,
                                         p.impl->frames[0].command_pool);
    recordCaptureCommands({cmd, p.src_image, staging_buf, p.width, p.height});
    submitAndWait(p.impl->device, p.impl->graphics_queue, cmd);
  }

  /// Execute GPU-side capture: record commands, submit, and readback pixels.
  std::vector<uint8_t> executeCapture(const CaptureParams& p,
                                      const RhiCaptureRequest& req) {
    auto pixel_size =
        static_cast<VkDeviceSize>(p.width) * p.height * CAPTURE_CHANNELS;
    auto staging = createCaptureStagingBuffer(p.impl->allocator, pixel_size);
    submitCaptureCommands(p, staging.buffer);
    ReadbackParams rb{p.impl->allocator, &staging, p.width, p.height};
    auto encoded = readbackAndEncode(rb, req);
    vmaDestroyBuffer(p.impl->allocator, staging.buffer, staging.allocation);
    return encoded;
  }

  // --- Shader validation helpers ---

  /// Check that SPIR-V bytecode is present and 4-byte aligned.
  bool isValidSpirv(const RhiShaderDesc& desc) {
    if (desc.bytecode == nullptr || desc.bytecode_size == 0) {
      return false;
    }
    auto addr = reinterpret_cast<uintptr_t>(desc.bytecode);
    return addr % alignof(uint32_t) == 0;
  }

  /// Build a VkShaderModuleCreateInfo from an RhiShaderDesc.
  VkShaderModuleCreateInfo
  buildShaderModuleCreateInfo(const RhiShaderDesc& desc) {
    VkShaderModuleCreateInfo info{};
    info.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    info.codeSize = desc.bytecode_size;
    info.pCode = reinterpret_cast<const uint32_t*>(desc.bytecode);
    return info;
  }

  // --- Pipeline creation helpers ---

  VkPipelineShaderStageCreateInfo
  buildShaderStageInfo(VkShaderModule module, VkShaderStageFlagBits stage) {
    VkPipelineShaderStageCreateInfo info{};
    info.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    info.stage = stage;
    info.module = module;
    info.pName = "main";
    return info;
  }

  void
  buildVertexInputState(const RhiVertexLayout& layout,
                        std::vector<VkVertexInputAttributeDescription>& attrs,
                        VkVertexInputBindingDescription& binding) {
    binding.binding = 0;
    binding.stride = layout.stride;
    binding.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;
    attrs.resize(layout.attribute_count);
    for (uint32_t i = 0; i < layout.attribute_count; ++i) {
      attrs[i].location = layout.attributes[i].location;
      attrs[i].binding = 0;
      attrs[i].format = toVkFormat(layout.attributes[i].format);
      attrs[i].offset = layout.attributes[i].offset;
    }
  }

  VkPipelineInputAssemblyStateCreateInfo
  buildInputAssemblyState(RhiPrimitiveTopology topo) {
    VkPipelineInputAssemblyStateCreateInfo info{};
    info.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
    info.topology = toVkTopology(topo);
    info.primitiveRestartEnable = VK_FALSE;
    return info;
  }

  VkPipelineRasterizationStateCreateInfo
  buildRasterState(const RhiRasterState& raster) {
    VkPipelineRasterizationStateCreateInfo info{};
    info.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
    info.polygonMode =
        raster.wireframe ? VK_POLYGON_MODE_LINE : VK_POLYGON_MODE_FILL;
    info.cullMode =
        raster.cull_back ? VK_CULL_MODE_BACK_BIT : VK_CULL_MODE_NONE;
    info.frontFace = raster.front_ccw ? VK_FRONT_FACE_COUNTER_CLOCKWISE
                                      : VK_FRONT_FACE_CLOCKWISE;
    info.lineWidth = 1.0f;
    return info;
  }

  VkPipelineDepthStencilStateCreateInfo
  buildDepthStencilState(const RhiDepthStencilState& ds) {
    VkPipelineDepthStencilStateCreateInfo info{};
    info.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
    info.depthTestEnable = ds.depth_test ? VK_TRUE : VK_FALSE;
    info.depthWriteEnable = ds.depth_write ? VK_TRUE : VK_FALSE;
    info.depthCompareOp = VK_COMPARE_OP_LESS;
    return info;
  }

  VkPipelineColorBlendAttachmentState
  buildColorBlendAttachment(const RhiBlendState& blend) {
    VkPipelineColorBlendAttachmentState state{};
    state.blendEnable = blend.enabled ? VK_TRUE : VK_FALSE;
    state.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |
                           VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
    return state;
  }

  VkPipelineLayout createEmptyPipelineLayout(VkDevice device) {
    VkPipelineLayoutCreateInfo info{};
    info.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    VkPipelineLayout layout = VK_NULL_HANDLE;
    if (vkCreatePipelineLayout(device, &info, nullptr, &layout) != VK_SUCCESS) {
      return VK_NULL_HANDLE;
    }
    return layout;
  }

  // --- Graphics pipeline decomposition ---

  /// Number of shader stages in a standard graphics pipeline (vertex +
  /// fragment).
  constexpr uint32_t GRAPHICS_SHADER_STAGE_COUNT = 2;

  /// Number of dynamic states (viewport + scissor).
  constexpr uint32_t DYNAMIC_STATE_COUNT = 2;

  /// Intermediate state for building a graphics pipeline.
  struct GraphicsPipelineState {
    /// Shader stages (vertex + fragment).
    std::array<VkPipelineShaderStageCreateInfo, GRAPHICS_SHADER_STAGE_COUNT>
        stages{};
    /// Vertex attribute descriptions.
    std::vector<VkVertexInputAttributeDescription> attrs{};
    /// Vertex binding description.
    VkVertexInputBindingDescription binding{};
    /// Vertex input state.
    VkPipelineVertexInputStateCreateInfo vertex_input{};
    /// Input assembly state.
    VkPipelineInputAssemblyStateCreateInfo input_assembly{};
    /// Rasterization state.
    VkPipelineRasterizationStateCreateInfo raster{};
    /// Depth/stencil state.
    VkPipelineDepthStencilStateCreateInfo depth_stencil{};
    /// Blend attachment state.
    VkPipelineColorBlendAttachmentState blend_attachment{};
    /// Blend state.
    VkPipelineColorBlendStateCreateInfo blend_state{};
    /// Multisample state.
    VkPipelineMultisampleStateCreateInfo multisample{};
    /// Dynamic state values.
    std::array<VkDynamicState, DYNAMIC_STATE_COUNT> dynamic_state_values{};
    /// Dynamic state create info.
    VkPipelineDynamicStateCreateInfo dynamic_state{};
    /// Viewport state.
    VkPipelineViewportStateCreateInfo viewport_state{};
    /// Color attachment format.
    VkFormat color_fmt = VK_FORMAT_UNDEFINED;
    /// Depth attachment format.
    VkFormat depth_fmt = VK_FORMAT_UNDEFINED;
    /// Dynamic rendering info.
    VkPipelineRenderingCreateInfo rendering_info{};
  };

  /// Populate shader stages in the pipeline state.
  void populateGraphicsShaderStages(GraphicsPipelineState& s, VkShaderModule vs,
                                    VkShaderModule fs) {
    s.stages[0] = buildShaderStageInfo(vs, VK_SHADER_STAGE_VERTEX_BIT);
    s.stages[1] = buildShaderStageInfo(fs, VK_SHADER_STAGE_FRAGMENT_BIT);
  }

  /// Populate vertex input state from the RHI vertex layout.
  void populateGraphicsVertexInput(GraphicsPipelineState& s,
                                   const RhiVertexLayout& layout) {
    buildVertexInputState(layout, s.attrs, s.binding);
    s.vertex_input.sType =
        VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
    s.vertex_input.vertexBindingDescriptionCount = 1;
    s.vertex_input.pVertexBindingDescriptions = &s.binding;
    s.vertex_input.vertexAttributeDescriptionCount =
        static_cast<uint32_t>(s.attrs.size());
    s.vertex_input.pVertexAttributeDescriptions = s.attrs.data();
  }

  /// Populate multisample and dynamic state.
  void populateGraphicsDynamicState(GraphicsPipelineState& s) {
    s.multisample.sType =
        VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
    s.multisample.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
    s.dynamic_state_values = {VK_DYNAMIC_STATE_VIEWPORT,
                              VK_DYNAMIC_STATE_SCISSOR};
    s.dynamic_state.sType =
        VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
    s.dynamic_state.dynamicStateCount =
        static_cast<uint32_t>(s.dynamic_state_values.size());
    s.dynamic_state.pDynamicStates = s.dynamic_state_values.data();
  }

  /// Populate viewport state with dynamic viewport/scissor.
  void populateGraphicsViewportState(GraphicsPipelineState& s) {
    s.viewport_state.sType =
        VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
    s.viewport_state.viewportCount = 1;
    s.viewport_state.scissorCount = 1;
  }

  /// Populate fixed-function pipeline states.
  void populateGraphicsFixedFunction(GraphicsPipelineState& s,
                                     const RhiGraphicsPipelineDesc& desc) {
    s.input_assembly = buildInputAssemblyState(desc.topology);
    s.raster = buildRasterState(desc.raster);
    s.depth_stencil = buildDepthStencilState(desc.depth_stencil);
    s.blend_attachment = buildColorBlendAttachment(desc.blend);
    s.blend_state.sType =
        VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
    s.blend_state.attachmentCount = 1;
    s.blend_state.pAttachments = &s.blend_attachment;
    populateGraphicsDynamicState(s);
    populateGraphicsViewportState(s);
  }

  /// Populate dynamic rendering attachment formats.
  void populateGraphicsDynamicRendering(GraphicsPipelineState& s,
                                        const RhiGraphicsPipelineDesc& desc) {
    s.color_fmt = toVkFormat(desc.color_format);
    s.depth_fmt = toVkFormat(desc.depth_format);
    s.rendering_info.sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO;
    s.rendering_info.colorAttachmentCount = 1;
    s.rendering_info.pColorAttachmentFormats = &s.color_fmt;
    s.rendering_info.depthAttachmentFormat = s.depth_fmt;
  }

  // Named algorithm: buildGraphicsPipelineCreateInfo
  // Stateless assembly of a VkGraphicsPipelineCreateInfo from pre-populated
  // GraphicsPipelineState. No side effects; pure struct construction.
  VkGraphicsPipelineCreateInfo
  buildGraphicsPipelineCreateInfo(const GraphicsPipelineState& s,
                                  VkPipelineLayout layout) {
    VkGraphicsPipelineCreateInfo info{};
    info.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
    info.pNext = &s.rendering_info;
    info.stageCount = static_cast<uint32_t>(s.stages.size());
    info.pStages = s.stages.data();
    info.pVertexInputState = &s.vertex_input;
    info.pInputAssemblyState = &s.input_assembly;
    info.pViewportState = &s.viewport_state;
    info.pRasterizationState = &s.raster;
    info.pMultisampleState = &s.multisample;
    info.pDepthStencilState = &s.depth_stencil;
    info.pColorBlendState = &s.blend_state;
    info.pDynamicState = &s.dynamic_state;
    info.layout = layout;
    info.renderPass = VK_NULL_HANDLE;
    return info;
  }

  /// Submit a VkPipeline create call and insert into the handle table.
  RhiPipelineHandle
  submitGraphicsPipeline(VkDevice device, const GraphicsPipelineState& s,
                         VkPipelineLayout layout,
                         VulkanHandleTable<VulkanPipeline>& pipelines) {
    auto info = buildGraphicsPipelineCreateInfo(s, layout);
    VulkanPipeline vk_pipeline{};
    vk_pipeline.layout = layout;
    vk_pipeline.bind_point = VK_PIPELINE_BIND_POINT_GRAPHICS;
    VkResult result = vkCreateGraphicsPipelines(
        device, VK_NULL_HANDLE, 1, &info, nullptr, &vk_pipeline.pipeline);
    if (result != VK_SUCCESS) {
      vkDestroyPipelineLayout(device, layout, nullptr);
      return RHI_PIPELINE_INVALID;
    }
    return pipelines.insert(std::move(vk_pipeline));
  }

  /// Assemble and create the final VkPipeline from prepared state.
  RhiPipelineHandle
  finalizeGraphicsPipeline(VkDevice device, const GraphicsPipelineState& s,
                           VulkanHandleTable<VulkanPipeline>& pipelines) {
    VkPipelineLayout layout = createEmptyPipelineLayout(device);
    if (layout == VK_NULL_HANDLE) {
      return RHI_PIPELINE_INVALID;
    }
    return submitGraphicsPipeline(device, s, layout, pipelines);
  }

  /// Build a VkComputePipelineCreateInfo for a given shader and layout.
  VkComputePipelineCreateInfo
  buildComputePipelineCreateInfo(VkShaderModule module,
                                 VkPipelineLayout layout) {
    VkComputePipelineCreateInfo info{};
    info.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
    info.stage = buildShaderStageInfo(module, VK_SHADER_STAGE_COMPUTE_BIT);
    info.layout = layout;
    return info;
  }

  /// Submit a compute pipeline create call and insert into the handle table.
  RhiPipelineHandle submitComputePipeline(
      VkDevice device, const VkComputePipelineCreateInfo& info,
      VkPipelineLayout layout, VulkanHandleTable<VulkanPipeline>& pipelines) {
    VulkanPipeline vk_pipeline{};
    vk_pipeline.layout = layout;
    vk_pipeline.bind_point = VK_PIPELINE_BIND_POINT_COMPUTE;
    VkResult result = vkCreateComputePipelines(device, VK_NULL_HANDLE, 1, &info,
                                               nullptr, &vk_pipeline.pipeline);
    if (result != VK_SUCCESS) {
      vkDestroyPipelineLayout(device, layout, nullptr);
      return RHI_PIPELINE_INVALID;
    }
    return pipelines.insert(std::move(vk_pipeline));
  }

  /// Build and create a compute pipeline.
  RhiPipelineHandle
  finalizeComputePipeline(VkDevice device, VkShaderModule module,
                          VulkanHandleTable<VulkanPipeline>& pipelines) {
    VkPipelineLayout layout = createEmptyPipelineLayout(device);
    if (layout == VK_NULL_HANDLE) {
      return RHI_PIPELINE_INVALID;
    }
    auto info = buildComputePipelineCreateInfo(module, layout);
    return submitComputePipeline(device, info, layout, pipelines);
  }

}  // namespace

// ---------------------------------------------------------------------------
// Construction / Destruction
// ---------------------------------------------------------------------------

VulkanDevice::VulkanDevice() : impl_(std::make_unique<Impl>()) {}

VulkanDevice::~VulkanDevice() {
  if (!impl_) {
    return;
  }
  waitIdle();
  impl_->teardown();
}

// ---------------------------------------------------------------------------
// Factory
// ---------------------------------------------------------------------------

std::optional<std::unique_ptr<VulkanDevice>>
VulkanDevice::create(const RenderConfig& config) {
  auto device = std::unique_ptr<VulkanDevice>(new VulkanDevice());
  device->impl_->config = config;
  if (!device->impl_->initAll()) {
    return std::nullopt;
  }
  return device;
}

// ---------------------------------------------------------------------------
// Identification
// ---------------------------------------------------------------------------

RhiBackend VulkanDevice::backend() const {
  return RhiBackend::VULKAN;
}

const RhiDeviceCapabilities& VulkanDevice::capabilities() const {
  return impl_->caps;
}

// ---------------------------------------------------------------------------
// Buffer lifecycle
// ---------------------------------------------------------------------------

RhiBufferHandle VulkanDevice::createBuffer(const RhiBufferDesc& desc) {
  auto buf_info = buildBufferCreateInfo(desc);
  auto alloc_info = buildBufferAllocInfo(desc.host_visible ? HostVisible::YES
                                                           : HostVisible::NO);

  VulkanBuffer vk_buf{};
  vk_buf.host_visible = desc.host_visible;
  VkResult result =
      vmaCreateBuffer(impl_->allocator, &buf_info, &alloc_info, &vk_buf.buffer,
                      &vk_buf.allocation, nullptr);
  if (result != VK_SUCCESS) {
    return RHI_BUFFER_INVALID;
  }
  return impl_->buffers.insert(std::move(vk_buf));
}

void VulkanDevice::destroyBuffer(RhiBufferHandle handle) {
  auto removed = impl_->buffers.remove(handle);
  if (!removed.has_value()) {
    return;
  }
  vmaDestroyBuffer(impl_->allocator, removed->buffer, removed->allocation);
}

void* VulkanDevice::mapBuffer(RhiBufferHandle handle) {
  auto* buf = impl_->buffers.lookup(handle);
  if (buf == nullptr || !buf->host_visible) {
    return nullptr;
  }
  void* data = nullptr;
  vmaMapMemory(impl_->allocator, buf->allocation, &data);
  return data;
}

void VulkanDevice::unmapBuffer(RhiBufferHandle handle) {
  auto* buf = impl_->buffers.lookup(handle);
  if (buf == nullptr) {
    return;
  }
  vmaUnmapMemory(impl_->allocator, buf->allocation);
}

// ---------------------------------------------------------------------------
// Texture lifecycle
// ---------------------------------------------------------------------------

RhiTextureHandle VulkanDevice::createTexture(const RhiTextureDesc& desc) {
  auto img_info = buildImageCreateInfo(desc);
  auto vk_tex = allocateImage(impl_->allocator, img_info, desc);
  if (vk_tex.image == VK_NULL_HANDLE) {
    return RHI_TEXTURE_INVALID;
  }
  if (!impl_->createImageView(vk_tex)) {
    vmaDestroyImage(impl_->allocator, vk_tex.image, vk_tex.allocation);
    return RHI_TEXTURE_INVALID;
  }
  return impl_->textures.insert(std::move(vk_tex));
}

void VulkanDevice::destroyTexture(RhiTextureHandle handle) {
  auto removed = impl_->textures.remove(handle);
  if (!removed.has_value()) {
    return;
  }
  vkDestroyImageView(impl_->device, removed->view, nullptr);
  vmaDestroyImage(impl_->allocator, removed->image, removed->allocation);
}

// ---------------------------------------------------------------------------
// Texture update
// ---------------------------------------------------------------------------

namespace {

  /// Validate updateTexture2D parameters against the texture entry.
  bool isValidTextureUpdate(const VulkanTexture& tex,
                            const RhiTextureUpdate2D& u) {
    if (u.pixels == nullptr || u.width == 0 || u.height == 0) {
      return false;
    }
    if (u.format == RhiFormat::UNDEFINED || u.format != tex.rhi_format) {
      return false;
    }
    if (bytesPerTexel(u.format) == 0) {
      return false;
    }
    return (u.offset_x + u.width <= tex.width) &&
           (u.offset_y + u.height <= tex.height);
  }

  /// Compute row byte stride, defaulting to width × bpp when zero.
  uint32_t resolveRowBytes(const RhiTextureUpdate2D& u, uint32_t bpp) {
    if (u.bytes_per_row != 0) {
      return u.bytes_per_row;
    }
    return u.width * bpp;
  }

  /// Record a staging-to-image copy via a one-shot command buffer.
  void recordStagingToImageCopy(VkCommandBuffer cmd, VkBuffer staging,
                                const VulkanTexture& tex,
                                const RhiTextureUpdate2D& u) {
    VkBufferImageCopy region{};
    region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    region.imageSubresource.layerCount = 1;
    region.imageOffset = {static_cast<int32_t>(u.offset_x),
                          static_cast<int32_t>(u.offset_y), 0};
    region.imageExtent = {u.width, u.height, 1};
    vkCmdCopyBufferToImage(cmd, staging, tex.image,
                           VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region);
  }

}  // namespace

namespace {

  /// Staging buffer for texture upload.
  struct StagingAlloc {
    /// Vulkan buffer handle.
    VkBuffer buffer = VK_NULL_HANDLE;
    /// VMA allocation backing this staging buffer.
    VmaAllocation allocation = VK_NULL_HANDLE;
  };

  /// Create a CPU-visible staging buffer of the given size.
  StagingAlloc createStagingBuffer(VmaAllocator alloc, VkDeviceSize size) {
    VkBufferCreateInfo buf_info{VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO};
    buf_info.size = size;
    buf_info.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
    VmaAllocationCreateInfo alloc_info{};
    alloc_info.usage = VMA_MEMORY_USAGE_CPU_ONLY;
    StagingAlloc result{};
    vmaCreateBuffer(alloc, &buf_info, &alloc_info, &result.buffer,
                    &result.allocation, nullptr);
    return result;
  }

  /// Copy CPU pixels into a mapped staging buffer.
  void uploadToStaging(VmaAllocator alloc, VmaAllocation staging_alloc,
                       const void* pixels, VkDeviceSize size) {
    void* mapped = nullptr;
    vmaMapMemory(alloc, staging_alloc, &mapped);
    std::memcpy(mapped, pixels, size);
    vmaUnmapMemory(alloc, staging_alloc);
  }

  /// Allocate a one-time-submit command buffer from the current frame's pool.
  VkCommandBuffer allocateOneShotCmd(VulkanDevice::Impl& impl) {
    auto& frame = impl.frames[impl.frame_index];
    VkCommandBufferAllocateInfo cmd_ai{
        VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO};
    cmd_ai.commandPool = frame.command_pool;
    cmd_ai.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    cmd_ai.commandBufferCount = 1;
    VkCommandBuffer cmd = VK_NULL_HANDLE;
    vkAllocateCommandBuffers(impl.device, &cmd_ai, &cmd);
    return cmd;
  }

  /// Begin a one-shot command buffer for recording.
  VkCommandBuffer beginOneShotCopyCmd(VulkanDevice::Impl& impl) {
    VkCommandBuffer cmd = allocateOneShotCmd(impl);
    VkCommandBufferBeginInfo begin{VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO};
    begin.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    vkBeginCommandBuffer(cmd, &begin);
    return cmd;
  }

  /// Submit a completed command buffer and wait for the queue to become idle.
  void submitCopyAndWait(VulkanDevice::Impl& impl, VkCommandBuffer cmd) {
    vkEndCommandBuffer(cmd);
    VkSubmitInfo submit_info{VK_STRUCTURE_TYPE_SUBMIT_INFO};
    submit_info.commandBufferCount = 1;
    submit_info.pCommandBuffers = &cmd;
    vkQueueSubmit(impl.graphics_queue, 1, &submit_info, VK_NULL_HANDLE);
    vkQueueWaitIdle(impl.graphics_queue);
  }

  /// Submit a one-shot copy command and wait for completion.
  void submitOneShotCopy(VulkanDevice::Impl& impl, VkBuffer staging,
                         const VulkanTexture& tex,
                         const RhiTextureUpdate2D& update) {
    VkCommandBuffer cmd = beginOneShotCopyCmd(impl);
    recordStagingToImageCopy(cmd, staging, tex, update);
    submitCopyAndWait(impl, cmd);
  }

  /// Upload staging data to a texture and clean up the staging buffer.
  void uploadAndCleanup(VulkanDevice::Impl& impl, StagingAlloc staging,
                        const VulkanTexture& tex,
                        const RhiTextureUpdate2D& update) {
    submitOneShotCopy(impl, staging.buffer, tex, update);
    vmaDestroyBuffer(impl.allocator, staging.buffer, staging.allocation);
  }

  /// Compute total byte size for a 2D texture upload region.
  VkDeviceSize computeUploadSize(const RhiTextureUpdate2D& u) {
    auto bpp = bytesPerTexel(u.format);
    // NOLINTNEXTLINE(*-narrowing-conversions) — FP without vulkan.h
    return static_cast<VkDeviceSize>(resolveRowBytes(u, bpp)) * u.height;
  }

}  // namespace

bool VulkanDevice::updateTexture2D(RhiTextureHandle handle,
                                   const RhiTextureUpdate2D& update) {
  auto* tex = impl_->textures.lookup(handle);
  if (tex == nullptr || !isValidTextureUpdate(*tex, update)) {
    return false;
  }
  const VkDeviceSize size = computeUploadSize(update);
  auto staging = createStagingBuffer(impl_->allocator, size);
  if (staging.buffer == VK_NULL_HANDLE) {
    return false;
  }
  uploadToStaging(impl_->allocator, staging.allocation, update.pixels, size);
  uploadAndCleanup(*impl_, staging, *tex, update);
  return true;
}

// ---------------------------------------------------------------------------
// Shader lifecycle
// ---------------------------------------------------------------------------

RhiShaderHandle VulkanDevice::createShader(const RhiShaderDesc& desc) {
  if (!isValidSpirv(desc)) {
    return RHI_SHADER_INVALID;
  }
  auto info = buildShaderModuleCreateInfo(desc);
  VulkanShader vk_shader{};
  if (vkCreateShaderModule(impl_->device, &info, nullptr, &vk_shader.module) !=
      VK_SUCCESS) {
    return RHI_SHADER_INVALID;
  }
  return impl_->shaders.insert(std::move(vk_shader));
}

void VulkanDevice::destroyShader(RhiShaderHandle handle) {
  auto removed = impl_->shaders.remove(handle);
  if (!removed.has_value()) {
    return;
  }
  vkDestroyShaderModule(impl_->device, removed->module, nullptr);
}

// ---------------------------------------------------------------------------
// Pipeline lifecycle
// ---------------------------------------------------------------------------

RhiPipelineHandle
VulkanDevice::createGraphicsPipeline(const RhiGraphicsPipelineDesc& desc) {
  auto* vs = impl_->shaders.lookup(desc.vertex_shader);
  auto* fs = impl_->shaders.lookup(desc.fragment_shader);
  if (vs == nullptr || fs == nullptr) {
    return RHI_PIPELINE_INVALID;
  }
  GraphicsPipelineState state;
  populateGraphicsShaderStages(state, vs->module, fs->module);
  populateGraphicsVertexInput(state, desc.vertex_layout);
  populateGraphicsFixedFunction(state, desc);
  populateGraphicsDynamicRendering(state, desc);
  return finalizeGraphicsPipeline(impl_->device, state, impl_->pipelines);
}

RhiPipelineHandle
VulkanDevice::createComputePipeline(const RhiComputePipelineDesc& desc) {
  auto* cs = impl_->shaders.lookup(desc.compute_shader);
  if (cs == nullptr) {
    return RHI_PIPELINE_INVALID;
  }
  return finalizeComputePipeline(impl_->device, cs->module, impl_->pipelines);
}

void VulkanDevice::destroyPipeline(RhiPipelineHandle handle) {
  auto removed = impl_->pipelines.remove(handle);
  if (!removed.has_value()) {
    return;
  }
  vkDestroyPipeline(impl_->device, removed->pipeline, nullptr);
  if (removed->layout != VK_NULL_HANDLE) {
    vkDestroyPipelineLayout(impl_->device, removed->layout, nullptr);
  }
}

// ---------------------------------------------------------------------------
// Swap chain accessors
// ---------------------------------------------------------------------------

RhiTextureHandle VulkanDevice::backbufferTexture() const {
  return static_cast<RhiTextureHandle>(impl_->image_index + 1);
}

uint32_t VulkanDevice::backbufferWidth() const {
  return impl_->swapchain_extent.width;
}

uint32_t VulkanDevice::backbufferHeight() const {
  return impl_->swapchain_extent.height;
}

void VulkanDevice::resizeSwapchain(uint32_t width, uint32_t height) {
  if (width == 0U || height == 0U) {
    return;
  }
  waitIdle();
  impl_->config.backbuffer_width = width;
  impl_->config.backbuffer_height = height;
  impl_->recreateSwapchain();
}

// ---------------------------------------------------------------------------
// Frame management
// ---------------------------------------------------------------------------

bool VulkanDevice::beginFrame() {
  auto& frame = impl_->frames[impl_->frame_index];
  vkWaitForFences(impl_->device, 1, &frame.in_flight_fence, VK_TRUE,
                  UINT64_MAX);
  vkResetFences(impl_->device, 1, &frame.in_flight_fence);
  if (!impl_->acquireNextImage(frame)) {
    return false;
  }
  vkResetCommandPool(impl_->device, frame.command_pool, 0);
  return true;
}

void VulkanDevice::endFrame() {}

void VulkanDevice::submit(RhiCommandList& cmd) {
  auto& vcmd = static_cast<VulkanCommandList&>(cmd);
  auto& frame = impl_->frames[impl_->frame_index];
  VkCommandBuffer cmd_buf = vcmd.nativeCommandBuffer();
  VkPipelineStageFlags wait_stage =
      VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;

  auto info = buildSubmitInfo(&cmd_buf, &frame.image_available, &wait_stage,
                              &frame.render_finished);
  vkQueueSubmit(impl_->graphics_queue, 1, &info, frame.in_flight_fence);
}

bool VulkanDevice::present() {
  auto& frame = impl_->frames[impl_->frame_index];
  auto info = buildPresentInfo(&frame.render_finished, &impl_->swapchain,
                               &impl_->image_index);

  VkResult result = vkQueuePresentKHR(impl_->present_queue, &info);
  impl_->frame_index = (impl_->frame_index + 1) % FRAMES_IN_FLIGHT;

  // F6: SUBOPTIMAL is still usable; only OUT_OF_DATE signals loss
  if (result == VK_ERROR_OUT_OF_DATE_KHR) {
    return false;
  }
  return result == VK_SUCCESS || result == VK_SUBOPTIMAL_KHR;
}

// ---------------------------------------------------------------------------
// Command list creation
// ---------------------------------------------------------------------------

std::unique_ptr<RhiCommandList> VulkanDevice::createCommandList() {
  auto& frame = impl_->frames[impl_->frame_index];
  return std::make_unique<VulkanCommandList>(frame.command_buffer, *impl_);
}

// ---------------------------------------------------------------------------
// Capture
// ---------------------------------------------------------------------------

std::optional<RhiCaptureResult>
VulkanDevice::captureFramebuffer(const RhiCaptureRequest& request) {
  waitIdle();
  auto w = request.width > 0 ? request.width : impl_->swapchain_extent.width;
  auto h = request.height > 0 ? request.height : impl_->swapchain_extent.height;
  auto src_image = impl_->swapchain_images[impl_->image_index];
  CaptureParams cap{impl_.get(), src_image, w, h};
  auto encoded = executeCapture(cap, request);
  if (encoded.empty()) {
    return std::nullopt;
  }
  return RhiCaptureResult{std::move(encoded), w, h};
}

bool VulkanDevice::captureToFile(const RhiCaptureRequest& request,
                                 std::string_view path) {
  auto result = captureFramebuffer(request);
  if (!result.has_value()) {
    return false;
  }
  std::ofstream file(std::string(path), std::ios::binary);
  if (!file.is_open()) {
    return false;
  }
  file.write(reinterpret_cast<const char*>(result->data.data()),
             static_cast<std::streamsize>(result->data.size()));
  return file.good();
}

// ---------------------------------------------------------------------------
// Extensions
// ---------------------------------------------------------------------------

IRhiRayTracing* VulkanDevice::rayTracing() {
  return nullptr;
}

// ---------------------------------------------------------------------------
// Synchronization
// ---------------------------------------------------------------------------

void VulkanDevice::waitIdle() {
  if (impl_->device != VK_NULL_HANDLE) {
    vkDeviceWaitIdle(impl_->device);
  }
}

}  // namespace eng::render

#endif  // ENGINE_RENDERER_VULKAN
