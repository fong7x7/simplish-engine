#pragma once

#ifdef ENGINE_RENDERER_VULKAN

/// @file vulkan-image-transition.h
/// @brief Layout transitions for images the backend tracks the layout of.
/// @par Threading Main-thread-only.

#include "vulkan-image-ref.h"

#include <vulkan/vulkan.h>

namespace eng::render {

/// Record a barrier taking `ref` from the layout it is in to `to`, and
/// remember `to`. Nothing is recorded when it is already there or the ref
/// names no image. Must be recorded outside a render pass.
///
/// The stage and access masks come from the two layouts, so a transition
/// out of the colour attachment layout waits for colour writes and one
/// into the sampled layout makes them visible to fragment shaders.
void transitionVulkanImage(VkCommandBuffer cmd, const VulkanImageRef& ref,
                           VkImageLayout to);

/// Where an image waits between uses: sampled images in the shader-read
/// layout, so any draw can bind them without a barrier inside its pass;
/// anything else wherever it was last left.
VkImageLayout vulkanRestingLayout(const VulkanImageRef& ref);

}  // namespace eng::render

#endif  // ENGINE_RENDERER_VULKAN
