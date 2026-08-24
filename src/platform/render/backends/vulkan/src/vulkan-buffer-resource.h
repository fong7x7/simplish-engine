#pragma once

#ifdef ENGINE_RENDERER_VULKAN

#include <vk_mem_alloc.h>
#include <vulkan/vulkan.h>

namespace eng::render {

struct VulkanBuffer {
  /// Vulkan buffer object.
  VkBuffer buffer = VK_NULL_HANDLE;
  /// VMA allocation backing this buffer.
  VmaAllocation allocation = VK_NULL_HANDLE;
  /// Whether this buffer is CPU-mappable.
  bool host_visible = false;
};

}  // namespace eng::render

#endif  // ENGINE_RENDERER_VULKAN
