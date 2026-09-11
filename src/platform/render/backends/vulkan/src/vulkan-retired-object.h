#pragma once

#ifdef ENGINE_RENDERER_VULKAN

/// @file vulkan-retired-object.h
/// @brief A GPU object the RHI has destroyed but a frame in flight may
/// still be using.
/// @par Threading Main-thread-only.

#include <cstdint>
#include <vk_mem_alloc.h>
#include <vulkan/vulkan.h>

namespace eng::render {

/// What `destroyBuffer`, `destroyTexture` and `destroyPipeline` leave
/// behind until the frames that could still reference it have retired.
///
/// A Metal command buffer retains every object it encodes, so the renderers
/// free a buffer the moment they are done with it — the GUI regrows its
/// vertex buffer mid-stream, the mesh renderer drops its depth target on a
/// resize. Vulkan destroys on the spot, so the backend holds the handles
/// here and destroys them once the frame that was current has finished.
/// Whichever of the fields is set is destroyed; the rest stay null.
struct VulkanRetiredObject {
  /// Serial of the latest frame that could have recorded a use of it.
  uint64_t frame_serial = 0;
  /// Buffer to destroy.
  VkBuffer buffer = VK_NULL_HANDLE;
  /// Image to destroy.
  VkImage image = VK_NULL_HANDLE;
  /// View of `image` to destroy.
  VkImageView view = VK_NULL_HANDLE;
  /// Allocation backing `buffer` or `image`.
  VmaAllocation allocation = VK_NULL_HANDLE;
  /// Pipeline to destroy.
  VkPipeline pipeline = VK_NULL_HANDLE;
};

}  // namespace eng::render

#endif  // ENGINE_RENDERER_VULKAN
