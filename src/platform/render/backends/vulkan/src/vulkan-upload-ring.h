#pragma once

#ifdef ENGINE_RENDERER_VULKAN

/// @file vulkan-upload-ring.h
/// @brief Per-frame linear buffer backing the stage-bytes uniform ranges.
/// @par Threading Main-thread-only, like the rest of the Vulkan backend.

#include <cstdint>
#include <vk_mem_alloc.h>
#include <vulkan/vulkan.h>

namespace eng::render {

/// One frame's worth of small constants written by the CPU.
///
/// `setVertexStageBytes` and `setFragmentStageBytes` hand the backend a few
/// hundred bytes that have to stay put until the frame's commands retire.
/// Push constants would be the cheap answer, but a skinned mesh's joint
/// palette alone is 3840 bytes and Vulkan only promises 128 — so the bytes
/// land here and the command list pushes a uniform descriptor for the range
/// returned. `Dx12UploadRing` is the same idea for root constant buffers.
///
/// The buffer is reset, not freed, at the top of each frame, which is safe
/// because `beginFrame` has already waited on that frame's fence.
class VulkanUploadRing {
public:
  /// Allocate and map the buffer. `alignment` is the device's minimum
  /// uniform buffer offset alignment. False if the allocation fails.
  bool create(VmaAllocator allocator, VkDeviceSize capacity,
              VkDeviceSize alignment);

  /// Release the buffer and its allocation.
  void destroy(VmaAllocator allocator);

  /// Hand back every byte allocated since the last reset.
  void reset();

  /// Copy `size` bytes in and return the range to bind them at, whose
  /// buffer is null when this frame's budget is spent.
  VkDescriptorBufferInfo push(const void* data, VkDeviceSize size);

private:
  /// Host-visible, coherent buffer holding this frame's constants.
  VkBuffer buffer_ = VK_NULL_HANDLE;
  /// VMA allocation backing `buffer_`.
  VmaAllocation allocation_ = VK_NULL_HANDLE;
  /// Persistent CPU mapping of `buffer_`.
  uint8_t* mapped_ = nullptr;
  /// Bytes handed out so far this frame.
  VkDeviceSize cursor_ = 0;
  /// Total bytes in `buffer_`.
  VkDeviceSize capacity_ = 0;
  /// Every range starts on a multiple of this.
  VkDeviceSize alignment_ = 1;
};

}  // namespace eng::render

#endif  // ENGINE_RENDERER_VULKAN
