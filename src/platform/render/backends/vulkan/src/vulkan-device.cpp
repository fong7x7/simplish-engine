#include "vulkan-builtin-pipelines.h"
#include "vulkan-command-list.h"
#include "vulkan-device-impl.h"
#include "vulkan-format-map.h"
#include "vulkan-image-transition.h"

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
#include <utility>
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

  /// Every buffer can be copied to and from, as every Metal buffer can be
  /// blitted; `copyBuffer` would otherwise fail on half of them.
  VkBufferCreateInfo buildBufferCreateInfo(const RhiBufferDesc& desc) {
    VkBufferCreateInfo info{};
    info.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    info.size = desc.size;
    info.usage = toVkBufferUsage(desc.usage) |
                 VK_BUFFER_USAGE_TRANSFER_SRC_BIT |
                 VK_BUFFER_USAGE_TRANSFER_DST_BIT;
    return info;
  }

  /// Whether a buffer should be host-visible (CPU-mappable).
  enum class HostVisible { NO, YES };

  /// Host-visible buffers are coherent too, since `unmapBuffer` flushes
  /// nothing and the renderers write them with a plain memcpy.
  VmaAllocationCreateInfo buildBufferAllocInfo(HostVisible host_visible) {
    VmaAllocationCreateInfo info{};
    info.usage = host_visible == HostVisible::YES ? VMA_MEMORY_USAGE_CPU_TO_GPU
                                                  : VMA_MEMORY_USAGE_GPU_ONLY;
    if (host_visible == HostVisible::YES) {
      info.requiredFlags = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
                           VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;
    }
    return info;
  }

  /// Which aspects of an image of this format a view or barrier covers.
  VkImageAspectFlags aspectForFormat(RhiFormat fmt) {
    switch (fmt) {
      case RhiFormat::D16_UNORM:
      case RhiFormat::D32_FLOAT:
        return VK_IMAGE_ASPECT_DEPTH_BIT;
      case RhiFormat::D24_UNORM_S8_UINT:
      case RhiFormat::D32_FLOAT_S8_UINT:
        return VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT;
      default:
        return VK_IMAGE_ASPECT_COLOR_BIT;
    }
  }

  /// Transfer in both directions on top of what was asked for: uploads need
  /// the one, capture and `copyTextureToBuffer` the other, and Metal lets
  /// any texture be blitted either way.
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
    info.usage = toVkImageUsage(desc.usage) | VK_IMAGE_USAGE_TRANSFER_SRC_BIT |
                 VK_IMAGE_USAGE_TRANSFER_DST_BIT;
    info.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
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
    vk_tex.aspect = aspectForFormat(desc.format);
    vk_tex.usage = img_info.usage;
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
    alloc_info.requiredFlags = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
                               VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;

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

  bool isBgra(VkFormat format) {
    return format == VK_FORMAT_B8G8R8A8_SRGB ||
           format == VK_FORMAT_B8G8R8A8_UNORM;
  }

  /// Swap the red and blue channels in place. The swapchain is BGRA8 and
  /// the copy moves its bytes untouched, while PNG and JPEG both want RGBA;
  /// the Metal backend does the same for the same reason.
  void swizzleBgraToRgba(std::vector<uint8_t>& pixels) {
    for (size_t i = 0; i + 3 < pixels.size(); i += CAPTURE_CHANNELS) {
      std::swap(pixels[i], pixels[i + 2]);
    }
  }

  /// Record a full-image copy into a tightly packed buffer.
  void recordCopyImageToBuffer(VkCommandBuffer cmd, const VulkanImageRef& ref,
                               VkBuffer dst) {
    VkBufferImageCopy region{};
    region.imageSubresource = {ref.aspect, 0, 0, 1};
    region.imageExtent = {ref.extent.width, ref.extent.height, 1};
    vkCmdCopyImageToBuffer(cmd, ref.image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                           dst, 1, &region);
  }

  /// Copy a staging buffer's bytes out to the CPU.
  std::vector<uint8_t> readStaging(VmaAllocator alloc,
                                   const VulkanBuffer& staging,
                                   VkDeviceSize size) {
    void* mapped = nullptr;
    vmaMapMemory(alloc, staging.allocation, &mapped);
    const auto* bytes = static_cast<const uint8_t*>(mapped);
    std::vector<uint8_t> pixels(bytes, bytes + size);
    vmaUnmapMemory(alloc, staging.allocation);
    return pixels;
  }

  /// Copy the image into `staging` and wait, leaving the image in the
  /// layout it was found in.
  void copyImageOut(VulkanDevice::Impl& impl, const VulkanImageRef& ref,
                    VkBuffer staging) {
    const VkImageLayout resting = *ref.layout;
    VkCommandBuffer cmd = impl.beginOneShot();
    transitionVulkanImage(cmd, ref, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL);
    recordCopyImageToBuffer(cmd, ref, staging);
    transitionVulkanImage(cmd, ref, resting);
    impl.submitOneShot(cmd);
  }

  /// Whether capture may read `handle` now. A swapchain image belongs to
  /// the presentation engine from `present` until it is acquired again, so
  /// the back buffer is readable only between `beginFrame` and `present` —
  /// unlike Metal, whose drawable texture outlives its present.
  bool isReadableTarget(const VulkanDevice::Impl& impl,
                        RhiTextureHandle handle) {
    const auto sc_count =
        static_cast<RhiTextureHandle>(impl.swapchain_images.size());
    if (handle == 0 || handle > sc_count) {
      return true;
    }
    return impl.image_acquired &&
           handle == static_cast<RhiTextureHandle>(impl.image_index) + 1;
  }

  /// The image's texels as tightly packed four-byte pixels, or nothing when
  /// it has never been written or cannot be copied from.
  std::optional<std::vector<uint8_t>>
  readImagePixels(VulkanDevice::Impl& impl, const VulkanImageRef& ref) {
    if (ref.layout == nullptr || *ref.layout == VK_IMAGE_LAYOUT_UNDEFINED ||
        (ref.usage & VK_IMAGE_USAGE_TRANSFER_SRC_BIT) == 0) {
      return std::nullopt;
    }
    const VkDeviceSize size = static_cast<VkDeviceSize>(ref.extent.width) *
                              ref.extent.height * CAPTURE_CHANNELS;
    VulkanBuffer staging = createCaptureStagingBuffer(impl.allocator, size);
    if (staging.buffer == VK_NULL_HANDLE) {
      return std::nullopt;
    }
    copyImageOut(impl, ref, staging.buffer);
    auto pixels = readStaging(impl.allocator, staging, size);
    vmaDestroyBuffer(impl.allocator, staging.buffer, staging.allocation);
    return pixels;
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
  buildShaderStageInfo(const VulkanShader& shader,
                       VkShaderStageFlagBits stage) {
    VkPipelineShaderStageCreateInfo info{};
    info.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    info.stage = stage;
    info.module = shader.module;
    info.pName = shader.entry_point.c_str();
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

  /// Alpha blending as the Metal backend's `applyAlphaBlend` sets it: RGB
  /// by source alpha, alpha passed through.
  VkPipelineColorBlendAttachmentState
  buildColorBlendAttachment(const RhiBlendState& blend) {
    VkPipelineColorBlendAttachmentState state{};
    state.blendEnable = blend.enabled ? VK_TRUE : VK_FALSE;
    state.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
    state.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
    state.colorBlendOp = VK_BLEND_OP_ADD;
    state.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
    state.dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;
    state.alphaBlendOp = VK_BLEND_OP_ADD;
    state.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |
                           VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
    return state;
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
  void populateGraphicsShaderStages(GraphicsPipelineState& s,
                                    const VulkanShader& vs,
                                    const VulkanShader& fs) {
    s.stages[0] = buildShaderStageInfo(vs, VK_SHADER_STAGE_VERTEX_BIT);
    s.stages[1] = buildShaderStageInfo(fs, VK_SHADER_STAGE_FRAGMENT_BIT);
  }

  /// Populate vertex input state from the RHI vertex layout. A layout with
  /// no attributes binds no buffer, as Metal leaves out the descriptor.
  void populateGraphicsVertexInput(GraphicsPipelineState& s,
                                   const RhiVertexLayout& layout) {
    s.vertex_input.sType =
        VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
    if (layout.attribute_count == 0) {
      return;
    }
    buildVertexInputState(layout, s.attrs, s.binding);
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

  /// Insert a created pipeline into the handle table, or report failure.
  RhiPipelineHandle insertPipeline(VulkanDevice::Impl& impl,
                                   VkPipeline pipeline,
                                   VkPipelineBindPoint bind_point) {
    if (pipeline == VK_NULL_HANDLE) {
      return RHI_PIPELINE_INVALID;
    }
    VkPipelineLayout layout = bind_point == VK_PIPELINE_BIND_POINT_COMPUTE
                                  ? impl.shared_layout.compute
                                  : impl.shared_layout.graphics;
    return impl.pipelines.insert(VulkanPipeline{pipeline, layout, bind_point});
  }

  /// Create a graphics pipeline against the shared layout, so it takes
  /// stage bytes and a fragment texture the way the built-in ones do.
  RhiPipelineHandle finalizeGraphicsPipeline(VulkanDevice::Impl& impl,
                                             const GraphicsPipelineState& s) {
    auto info = buildGraphicsPipelineCreateInfo(s, impl.shared_layout.graphics);
    VkPipeline pipeline = VK_NULL_HANDLE;
    if (vkCreateGraphicsPipelines(impl.device, VK_NULL_HANDLE, 1, &info,
                                  nullptr, &pipeline) != VK_SUCCESS) {
      return RHI_PIPELINE_INVALID;
    }
    return insertPipeline(impl, pipeline, VK_PIPELINE_BIND_POINT_GRAPHICS);
  }

  /// Build and create a compute pipeline.
  RhiPipelineHandle finalizeComputePipeline(VulkanDevice::Impl& impl,
                                            const VulkanShader& shader) {
    VkComputePipelineCreateInfo info{};
    info.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
    info.stage = buildShaderStageInfo(shader, VK_SHADER_STAGE_COMPUTE_BIT);
    info.layout = impl.shared_layout.compute;
    VkPipeline pipeline = VK_NULL_HANDLE;
    if (vkCreateComputePipelines(impl.device, VK_NULL_HANDLE, 1, &info, nullptr,
                                 &pipeline) != VK_SUCCESS) {
      return RHI_PIPELINE_INVALID;
    }
    return insertPipeline(impl, pipeline, VK_PIPELINE_BIND_POINT_COMPUTE);
  }

  /// Hand a built-in pipeline to the caller; false if it failed to build.
  bool publishBuiltin(VulkanDevice::Impl& impl, VkPipeline pipeline,
                      RhiPipelineHandle& out) {
    const RhiPipelineHandle handle =
        insertPipeline(impl, pipeline, VK_PIPELINE_BIND_POINT_GRAPHICS);
    if (handle == RHI_PIPELINE_INVALID) {
      return false;
    }
    out = handle;
    return true;
  }

  // --- Texture upload helpers ---

  /// Validate updateTexture2D parameters against the texture entry, with
  /// the row-pitch rules `MetalRealDevice::updateTexture2D` applies.
  bool isValidTextureUpdate(const VulkanTexture& tex,
                            const RhiTextureUpdate2D& u) {
    if (u.pixels == nullptr || u.width == 0 || u.height == 0) {
      return false;
    }
    if (u.format == RhiFormat::UNDEFINED || u.format != tex.rhi_format) {
      return false;
    }
    const uint32_t bpp = bytesPerTexel(u.format);
    if (bpp == 0 || (u.bytes_per_row != 0 && (u.bytes_per_row < u.width * bpp ||
                                              u.bytes_per_row % bpp != 0))) {
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

  /// Record a staging-to-image copy, honouring a padded row pitch.
  void recordStagingToImageCopy(VkCommandBuffer cmd, VkBuffer staging,
                                const VulkanImageRef& ref,
                                const RhiTextureUpdate2D& u) {
    VkBufferImageCopy region{};
    region.bufferRowLength =
        resolveRowBytes(u, bytesPerTexel(u.format)) / bytesPerTexel(u.format);
    region.imageSubresource.aspectMask = ref.aspect;
    region.imageSubresource.layerCount = 1;
    region.imageOffset = {static_cast<int32_t>(u.offset_x),
                          static_cast<int32_t>(u.offset_y), 0};
    region.imageExtent = {u.width, u.height, 1};
    vkCmdCopyBufferToImage(cmd, staging, ref.image,
                           VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region);
  }

  /// Staging buffer for texture upload.
  struct StagingAlloc {
    /// Vulkan buffer handle.
    VkBuffer buffer = VK_NULL_HANDLE;
    /// VMA allocation backing this staging buffer.
    VmaAllocation allocation = VK_NULL_HANDLE;
  };

  /// Create a CPU-visible staging buffer of the given size.
  StagingAlloc createStagingBuffer(VmaAllocator alloc, VkDeviceSize size) {
    VkBufferCreateInfo buf_info{};
    buf_info.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
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

  /// Compute total byte size for a 2D texture upload region.
  VkDeviceSize computeUploadSize(const RhiTextureUpdate2D& u) {
    auto bpp = bytesPerTexel(u.format);
    // NOLINTNEXTLINE(*-narrowing-conversions) — FP without vulkan.h
    return static_cast<VkDeviceSize>(resolveRowBytes(u, bpp)) * u.height;
  }

  /// Copy `u` into the texture through a staging buffer, leaving it in its
  /// resting layout. Waits for the copy, as Metal's `replaceRegion` does.
  bool uploadPixels(VulkanDevice::Impl& impl, RhiTextureHandle handle,
                    const RhiTextureUpdate2D& u) {
    const VkDeviceSize size = computeUploadSize(u);
    const StagingAlloc staging = createStagingBuffer(impl.allocator, size);
    if (staging.buffer == VK_NULL_HANDLE) {
      return false;
    }
    uploadToStaging(impl.allocator, staging.allocation, u.pixels, size);
    const VulkanImageRef ref = impl.imageRef(handle);
    VkCommandBuffer cmd = impl.beginOneShot();
    transitionVulkanImage(cmd, ref, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
    recordStagingToImageCopy(cmd, staging.buffer, ref, u);
    transitionVulkanImage(cmd, ref, vulkanRestingLayout(ref));
    impl.submitOneShot(cmd);
    vmaDestroyBuffer(impl.allocator, staging.buffer, staging.allocation);
    return true;
  }

  /// The whole of a new texture, from its `initial_pixels`.
  RhiTextureUpdate2D fullTextureUpdate(const RhiTextureDesc& desc) {
    RhiTextureUpdate2D update{};
    update.pixels = desc.initial_pixels;
    update.width = desc.width;
    update.height = desc.height;
    update.format = desc.format;
    return update;
  }

  /// Give a new texture its pixels, or at least its resting layout, so a
  /// draw can sample it before anything is uploaded — a Metal texture is
  /// usable from the moment it exists.
  void settleNewTexture(VulkanDevice::Impl& impl, RhiTextureHandle handle,
                        const RhiTextureDesc& desc) {
    if (desc.initial_pixels != nullptr && bytesPerTexel(desc.format) != 0) {
      uploadPixels(impl, handle, fullTextureUpdate(desc));
      return;
    }
    const VulkanImageRef ref = impl.imageRef(handle);
    if ((ref.usage & VK_IMAGE_USAGE_SAMPLED_BIT) == 0) {
      return;
    }
    VkCommandBuffer cmd = impl.beginOneShot();
    transitionVulkanImage(cmd, ref, vulkanRestingLayout(ref));
    impl.submitOneShot(cmd);
  }

  /// One opaque white texel: what a draw samples when it bound no texture,
  /// so the shared layout's image binding is never left empty.
  RhiTextureHandle createStandInTexture(VulkanDevice& device) {
    static constexpr std::array<uint8_t, 4> WHITE{255, 255, 255, 255};
    RhiTextureDesc desc{};
    desc.format = RhiFormat::RGB_A8_UNORM;
    desc.usage = RhiTextureUsage::SAMPLED;
    desc.debug_name = "vulkan_stand_in";
    desc.initial_pixels = WHITE.data();
    return device.createTexture(desc);
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
  // The constructor is private so that create() is the only way in, which
  // puts make_unique out of reach; the pointer is owned before the
  // statement ends.
  auto device = std::unique_ptr<VulkanDevice>(
      new VulkanDevice());  // NOLINT(bare-new-delete) — private ctor, owned
                            // here
  device->impl_->config = config;
  if (!device->impl_->initAll()) {
    return std::nullopt;
  }
  device->impl_->stand_in_texture = createStandInTexture(*device);
  if (device->impl_->stand_in_texture == RHI_TEXTURE_INVALID) {
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
  return impl_->buffers.insert(vk_buf);
}

void VulkanDevice::destroyBuffer(RhiBufferHandle handle) {
  auto removed = impl_->buffers.remove(handle);
  if (!removed.has_value()) {
    return;
  }
  impl_->retire({0, removed->buffer, {}, {}, removed->allocation, {}});
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
  if (buf == nullptr || !buf->host_visible) {
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
  const RhiTextureHandle handle = impl_->textures.insert(vk_tex);
  settleNewTexture(*impl_, handle, desc);
  return handle;
}

void VulkanDevice::destroyTexture(RhiTextureHandle handle) {
  auto removed = impl_->textures.remove(handle);
  if (!removed.has_value()) {
    return;
  }
  impl_->retire(
      {0, {}, removed->image, removed->view, removed->allocation, {}});
}

bool VulkanDevice::updateTexture2D(RhiTextureHandle handle,
                                   const RhiTextureUpdate2D& update) {
  auto* tex = impl_->textures.lookup(handle);
  if (tex == nullptr || !isValidTextureUpdate(*tex, update)) {
    return false;
  }
  return uploadPixels(*impl_, handle, update);
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
  if (desc.entry_point != nullptr) {
    vk_shader.entry_point = desc.entry_point;
  }
  return impl_->shaders.insert(std::move(vk_shader));
}

void VulkanDevice::destroyShader(RhiShaderHandle handle) {
  // A module is only read while a pipeline is being created, so no frame
  // in flight can still need it.
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
  populateGraphicsShaderStages(state, *vs, *fs);
  populateGraphicsVertexInput(state, desc.vertex_layout);
  populateGraphicsFixedFunction(state, desc);
  populateGraphicsDynamicRendering(state, desc);
  return finalizeGraphicsPipeline(*impl_, state);
}

RhiPipelineHandle
VulkanDevice::createComputePipeline(const RhiComputePipelineDesc& desc) {
  auto* cs = impl_->shaders.lookup(desc.compute_shader);
  if (cs == nullptr) {
    return RHI_PIPELINE_INVALID;
  }
  return finalizeComputePipeline(*impl_, *cs);
}

void VulkanDevice::destroyPipeline(RhiPipelineHandle handle) {
  auto removed = impl_->pipelines.remove(handle);
  if (!removed.has_value()) {
    return;
  }
  // The layout is the device's shared one, so only the pipeline goes.
  impl_->retire({0, {}, {}, {}, {}, removed->pipeline});
}

// ---------------------------------------------------------------------------
// Built-in pipelines
// ---------------------------------------------------------------------------

bool VulkanDevice::tryCreateGuiPipeline(RhiPipelineHandle& out_pipeline) {
  return publishBuiltin(*impl_,
                        createVulkanGuiPipeline(impl_->device,
                                                impl_->shared_layout.graphics,
                                                impl_->swapchain_format),
                        out_pipeline);
}

bool VulkanDevice::tryCreateMeshPipeline(RhiPipelineHandle& out_pipeline) {
  return publishBuiltin(*impl_,
                        createVulkanMeshPipeline(impl_->device,
                                                 impl_->shared_layout.graphics,
                                                 impl_->swapchain_format),
                        out_pipeline);
}

bool VulkanDevice::tryCreateSkinnedMeshPipeline(
    RhiPipelineHandle& out_pipeline) {
  return publishBuiltin(*impl_,
                        createVulkanSkinnedMeshPipeline(
                            impl_->device, impl_->shared_layout.graphics,
                            impl_->swapchain_format),
                        out_pipeline);
}

bool VulkanDevice::tryCreateMeshOutlinePipeline(
    RhiPipelineHandle& out_pipeline) {
  return publishBuiltin(
      *impl_,
      createVulkanOutlinePipeline(impl_->device, impl_->shared_layout.graphics,
                                  impl_->swapchain_format),
      out_pipeline);
}

// ---------------------------------------------------------------------------
// Swap chain accessors
// ---------------------------------------------------------------------------

RhiTextureHandle VulkanDevice::backbufferTexture() const {
  return static_cast<RhiTextureHandle>(impl_->image_index) + 1;
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
  if (!impl_->acquireNextImage(frame)) {
    return false;
  }
  // Reset only with an image in hand: a frame that stops above submits
  // nothing, and a fence reset then would never be signalled again.
  impl_->image_acquired = true;
  vkResetFences(impl_->device, 1, &frame.in_flight_fence);
  vkResetCommandPool(impl_->device, frame.command_pool, 0);
  frame.stage_bytes.reset();
  // The fence waited on is this slot's last frame, FRAMES_IN_FLIGHT back;
  // it and everything before it are done.
  const uint64_t serial = ++impl_->frame_serial;
  impl_->releaseRetired(serial > FRAMES_IN_FLIGHT ? serial - FRAMES_IN_FLIGHT
                                                  : 0);
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
                              &impl_->present_ready[impl_->image_index]);
  vkQueueSubmit(impl_->graphics_queue, 1, &info, frame.in_flight_fence);
}

bool VulkanDevice::present() {
  auto info = buildPresentInfo(&impl_->present_ready[impl_->image_index],
                               &impl_->swapchain, &impl_->image_index);

  VkResult result = vkQueuePresentKHR(impl_->present_queue, &info);
  impl_->image_acquired = false;
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
  const RhiTextureHandle target = request.target != RHI_TEXTURE_INVALID
                                      ? request.target
                                      : backbufferTexture();
  if (!isReadableTarget(*impl_, target)) {
    return std::nullopt;
  }
  const VulkanImageRef ref = impl_->imageRef(target);
  auto pixels = readImagePixels(*impl_, ref);
  if (!pixels.has_value()) {
    return std::nullopt;
  }
  if (isBgra(ref.format)) {
    swizzleBgraToRgba(*pixels);
  }
  const uint32_t w = ref.extent.width;
  const uint32_t h = ref.extent.height;
  auto encoded = encodePixels(pixels->data(), w, h, request);
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
