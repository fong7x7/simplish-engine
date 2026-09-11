#pragma once

#ifdef ENGINE_RENDERER_VULKAN

#include <engine/render/rhi-types.h>
#include <vk_mem_alloc.h>
#include <vulkan/vulkan.h>

namespace eng::render {

struct VulkanTexture {
  /// Vulkan image object.
  VkImage image = VK_NULL_HANDLE;
  /// Vulkan image view for shader access.
  VkImageView view = VK_NULL_HANDLE;
  /// VMA allocation backing this image.
  VmaAllocation allocation = VK_NULL_HANDLE;
  /// Image width in texels.
  uint32_t width = 0;
  /// Image height in texels.
  uint32_t height = 0;
  /// Pixel format of the image (Vulkan).
  VkFormat format = VK_FORMAT_UNDEFINED;
  /// Original RHI format (for updateTexture2D validation).
  RhiFormat rhi_format = RhiFormat::UNDEFINED;
  /// Colour, or depth (and stencil) for a depth format.
  VkImageAspectFlags aspect = VK_IMAGE_ASPECT_COLOR_BIT;
  /// Usage the image was created with, the backend's additions included.
  VkImageUsageFlags usage = 0;
  /// The layout the backend last left the image in; see `VulkanImageRef`.
  VkImageLayout layout = VK_IMAGE_LAYOUT_UNDEFINED;
};

}  // namespace eng::render

#endif  // ENGINE_RENDERER_VULKAN
