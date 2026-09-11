#pragma once

#ifdef ENGINE_RENDERER_VULKAN

/// @file vulkan-stage-bindings.h
/// @brief The descriptors the next draw pushes.
/// @par Threading Main-thread-only, owned by one command list.

#include "vulkan-shared-layout.h"

#include <array>
#include <vulkan/vulkan.h>

namespace eng::render {

/// What `setVertexStageBytes`, `setFragmentStageBytes` and
/// `bindFragmentTexture` have set since the last draw pushed its set.
///
/// A Metal encoder keeps each slot's bytes and texture until they are set
/// again, and so does this: the whole set is pushed at the next draw after
/// any change, with the device's zeroed buffer and white texture standing
/// in for a slot that was never set.
struct VulkanStageBindings {
  /// Range bound at each uniform binding; a null buffer means unset.
  std::array<VkDescriptorBufferInfo, VULKAN_UNIFORM_BINDING_COUNT> uniforms{};
  /// View bound at the texture binding; null means unset.
  VkImageView texture = VK_NULL_HANDLE;
  /// Whether anything has changed since the set was last pushed.
  bool dirty = true;
};

}  // namespace eng::render

#endif  // ENGINE_RENDERER_VULKAN
