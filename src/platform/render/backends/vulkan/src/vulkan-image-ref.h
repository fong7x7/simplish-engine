#pragma once

#ifdef ENGINE_RENDERER_VULKAN

/// @file vulkan-image-ref.h
/// @brief One image an RHI texture handle names — a texture or a swapchain
/// image — with what a command needs to know to use it.
/// @par Threading Main-thread-only; a ref is valid for the call that got it.

#include <vulkan/vulkan.h>

namespace eng::render {

/// What `VulkanDevice::Impl::imageRef` resolves a texture handle to.
///
/// The RHI never says which layout an image is in — the renderers call no
/// `textureBarrier`, because Metal tracks hazards itself and DX12 here
/// tracks resource states for them. So the Vulkan backend remembers each
/// image's layout and `layout` points at that record, for a transition to
/// read and update. It points into the device's tables, so a ref must not
/// outlive the command that asked for it.
struct VulkanImageRef {
  /// The image, or null when the handle names nothing.
  VkImage image = VK_NULL_HANDLE;
  /// View over the whole image, with `aspect`.
  VkImageView view = VK_NULL_HANDLE;
  /// Colour, or depth for a depth format (and stencil when it has one).
  VkImageAspectFlags aspect = VK_IMAGE_ASPECT_COLOR_BIT;
  /// Pixel format.
  VkFormat format = VK_FORMAT_UNDEFINED;
  /// Size in texels.
  VkExtent2D extent{};
  /// Usage the image was created with.
  VkImageUsageFlags usage = 0;
  /// The layout the backend last left the image in.
  VkImageLayout* layout = nullptr;
};

}  // namespace eng::render

#endif  // ENGINE_RENDERER_VULKAN
