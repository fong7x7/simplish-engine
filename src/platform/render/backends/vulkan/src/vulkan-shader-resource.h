#pragma once

#ifdef ENGINE_RENDERER_VULKAN

#include <vulkan/vulkan.h>

namespace eng::render {

struct VulkanShader {
  /// Vulkan shader module handle.
  VkShaderModule module = VK_NULL_HANDLE;
};

}  // namespace eng::render

#endif  // ENGINE_RENDERER_VULKAN
