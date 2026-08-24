#pragma once

#ifdef ENGINE_RENDERER_VULKAN

#include <vulkan/vulkan.h>

namespace eng::render {

struct VulkanPipeline {
  /// Vulkan pipeline object.
  VkPipeline pipeline = VK_NULL_HANDLE;
  /// Pipeline layout used by this pipeline.
  VkPipelineLayout layout = VK_NULL_HANDLE;
  /// Whether this is a graphics or compute pipeline.
  VkPipelineBindPoint bind_point = VK_PIPELINE_BIND_POINT_GRAPHICS;
};

}  // namespace eng::render

#endif  // ENGINE_RENDERER_VULKAN
