#include "vulkan-upload-ring.h"

#ifdef ENGINE_RENDERER_VULKAN

#include <cstring>

namespace eng::render {

namespace {

  VkDeviceSize alignUp(VkDeviceSize value, VkDeviceSize alignment) {
    return (value + alignment - 1) & ~(alignment - 1);
  }

  /// Persistently mapped and coherent, so a push is a memcpy and nothing
  /// has to be flushed before the frame is submitted.
  VmaAllocationCreateInfo buildRingAllocInfo() {
    VmaAllocationCreateInfo info{};
    info.usage = VMA_MEMORY_USAGE_AUTO;
    info.flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT |
                 VMA_ALLOCATION_CREATE_MAPPED_BIT;
    info.requiredFlags = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
                         VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;
    return info;
  }

}  // namespace

bool VulkanUploadRing::create(VmaAllocator allocator, VkDeviceSize capacity,
                              VkDeviceSize alignment) {
  VkBufferCreateInfo buf_info{};
  buf_info.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
  buf_info.size = capacity;
  buf_info.usage = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT;
  const VmaAllocationCreateInfo alloc_info = buildRingAllocInfo();
  VmaAllocationInfo mapped{};
  if (vmaCreateBuffer(allocator, &buf_info, &alloc_info, &buffer_, &allocation_,
                      &mapped) != VK_SUCCESS) {
    return false;
  }
  mapped_ = static_cast<uint8_t*>(mapped.pMappedData);
  capacity_ = capacity;
  alignment_ = alignment > 0 ? alignment : 1;
  return mapped_ != nullptr;
}

void VulkanUploadRing::destroy(VmaAllocator allocator) {
  if (buffer_ != VK_NULL_HANDLE) {
    vmaDestroyBuffer(allocator, buffer_, allocation_);
  }
  buffer_ = VK_NULL_HANDLE;
  allocation_ = VK_NULL_HANDLE;
  mapped_ = nullptr;
  capacity_ = 0;
  cursor_ = 0;
}

void VulkanUploadRing::reset() {
  cursor_ = 0;
}

VkDescriptorBufferInfo VulkanUploadRing::push(const void* data,
                                              VkDeviceSize size) {
  const VkDeviceSize offset = alignUp(cursor_, alignment_);
  if (mapped_ == nullptr || data == nullptr || size == 0) {
    return {};
  }
  if (offset + size > capacity_) {
    return {};
  }
  std::memcpy(mapped_ + offset, data, size);
  cursor_ = offset + size;
  return {buffer_, offset, size};
}

}  // namespace eng::render

#endif  // ENGINE_RENDERER_VULKAN
