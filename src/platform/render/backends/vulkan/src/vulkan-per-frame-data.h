#pragma once

#ifdef ENGINE_RENDERER_VULKAN

#include "vulkan-upload-ring.h"

#include <vulkan/vulkan.h>

namespace eng::render {

struct PerFrameData {
  /// Command pool for this frame's command buffers.
  VkCommandPool command_pool = VK_NULL_HANDLE;
  /// Primary command buffer for this frame.
  VkCommandBuffer command_buffer = VK_NULL_HANDLE;
  /// Signalled when the swapchain image is ready.
  VkSemaphore image_available = VK_NULL_HANDLE;
  /// Signalled when the frame's GPU work is complete.
  VkFence in_flight_fence = VK_NULL_HANDLE;
  /// Stage bytes written while recording this frame; reset once its fence
  /// has been waited on.
  VulkanUploadRing stage_bytes{};
};

}  // namespace eng::render

#endif  // ENGINE_RENDERER_VULKAN
