#pragma once

#ifdef ENGINE_RENDERER_VULKAN

/// @file vulkan-shared-layout.h
/// @brief The one pipeline layout every Vulkan graphics pipeline shares.
/// @par Threading Stateless; the creation calls are main-thread-only.

#include <cstdint>
#include <vulkan/vulkan.h>

namespace eng::render {

// ---------------------------------------------------------------------------
// Why one shared layout
//
// The RHI has no descriptor-set concept yet: a draw passes its constants
// through `setVertexStageBytes` / `setFragmentStageBytes` and its texture
// through `bindFragmentTexture`, each addressed by a small integer slot.
// That is a fixed shape, so it is described once here — set 0, one binding
// per slot — and every graphics pipeline is created against it, the
// built-in ones and those made from caller-supplied SPIR-V alike. It is
// `dx12-root-signature.h` restated for Vulkan.
//
// The set is a push descriptor set: the command list writes it straight
// into the command buffer before a draw, with no pool to allocate from and
// nothing to free. That is the closest Vulkan has to Metal's `setBytes`.
// The two samplers are immutable, so a draw never writes them.
// ---------------------------------------------------------------------------

/// Binding of the vertex stage's slot-0 uniform block.
inline constexpr uint32_t VULKAN_BINDING_VERTEX_UBO0 = 0;

/// Binding of the vertex stage's slot-1 uniform block.
inline constexpr uint32_t VULKAN_BINDING_VERTEX_UBO1 = 1;

/// Binding of the vertex stage's slot-2 uniform block, which is where the
/// skinned mesh shader reads its joint palette.
inline constexpr uint32_t VULKAN_BINDING_VERTEX_UBO2 = 2;

/// Binding of the fragment stage's slot-0 uniform block.
inline constexpr uint32_t VULKAN_BINDING_FRAGMENT_UBO0 = 3;

/// Binding of the fragment stage's slot-1 uniform block.
inline constexpr uint32_t VULKAN_BINDING_FRAGMENT_UBO1 = 4;

/// Number of uniform bindings; they are the first ones, numbered from 0.
inline constexpr uint32_t VULKAN_UNIFORM_BINDING_COUNT = 5;

/// Binding of the fragment stage's slot-0 sampled image.
inline constexpr uint32_t VULKAN_BINDING_FRAGMENT_TEXTURE = 5;

/// Binding of the linear, clamp-to-edge immutable sampler — what the GUI
/// wants for a quad sampled once across its own rect.
inline constexpr uint32_t VULKAN_BINDING_CLAMP_SAMPLER = 6;

/// Binding of the linear, repeating immutable sampler — what a mesh wants
/// so that a tiling map tiles.
inline constexpr uint32_t VULKAN_BINDING_REPEAT_SAMPLER = 7;

/// Number of bindings in the shared set.
inline constexpr uint32_t VULKAN_SHARED_BINDING_COUNT = 8;

/// Number of bindings a draw writes: the uniforms and the texture.
inline constexpr uint32_t VULKAN_PUSHED_BINDING_COUNT = 6;

/// Returned for a stage-bytes slot the layout has no binding for.
inline constexpr uint32_t VULKAN_BINDING_NONE = UINT32_MAX;

/// Binding for a vertex-stage slot, or `VULKAN_BINDING_NONE`.
inline uint32_t vulkanVertexUniformBinding(uint32_t slot) {
  return slot <= VULKAN_BINDING_VERTEX_UBO2 ? slot : VULKAN_BINDING_NONE;
}

/// Binding for a fragment-stage slot, or `VULKAN_BINDING_NONE`.
inline uint32_t vulkanFragmentUniformBinding(uint32_t slot) {
  return slot <= 1 ? VULKAN_BINDING_FRAGMENT_UBO0 + slot : VULKAN_BINDING_NONE;
}

/// The shared descriptor set layout, the samplers it holds, and the two
/// pipeline layouts built from it. Owned by the device.
struct VulkanSharedLayout {
  /// Linear sampler that clamps to the edge.
  VkSampler clamp_sampler = VK_NULL_HANDLE;
  /// Linear sampler that repeats.
  VkSampler repeat_sampler = VK_NULL_HANDLE;
  /// Push descriptor set layout describing the slots above.
  VkDescriptorSetLayout set_layout = VK_NULL_HANDLE;
  /// Layout of every graphics pipeline: the one set above.
  VkPipelineLayout graphics = VK_NULL_HANDLE;
  /// Layout of every compute pipeline, which binds nothing yet.
  VkPipelineLayout compute = VK_NULL_HANDLE;
};

/// Create the samplers, set layout and pipeline layouts. False on failure,
/// in which case whatever was created is still recorded in `out` for
/// `destroyVulkanSharedLayout` to release.
bool createVulkanSharedLayout(VkDevice device, VulkanSharedLayout& out);

/// Destroy everything `createVulkanSharedLayout` made.
void destroyVulkanSharedLayout(VkDevice device, VulkanSharedLayout& layout);

}  // namespace eng::render

#endif  // ENGINE_RENDERER_VULKAN
