#pragma once

#ifdef ENGINE_RENDERER_VULKAN

#include <string>
#include <vulkan/vulkan.h>

namespace eng::render {

struct VulkanShader {
  /// Vulkan shader module handle.
  VkShaderModule module = VK_NULL_HANDLE;
  /// Entry point the pipeline stage names, from `RhiShaderDesc`.
  std::string entry_point = "main";
};

}  // namespace eng::render

#endif  // ENGINE_RENDERER_VULKAN
