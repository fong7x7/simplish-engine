#pragma once

#ifdef ENGINE_RENDERER_VULKAN

/// @file vulkan-builtin-pipelines.h
/// @brief The GUI, mesh, skinned mesh, outline, effects and water pipelines the
/// backend ships itself.
/// @par Threading Main-thread-only.

#include <vulkan/vulkan.h>

namespace eng::render {

// ---------------------------------------------------------------------------
// These exist for the same reason the Metal backend compiles MSL at device
// creation: `createShader` takes compiled bytecode and the project has no
// shader build step yet, so the GUI and the mesh renderers would have
// nothing to draw with. The GLSL lives in the .cpp beside them and mirrors
// the MSL in `metal-device-impl.mm` and the HLSL in
// `dx12-builtin-pipelines.cpp` — the three are meant to shade alike, so a
// change to one belongs in the others.
//
// Every pipeline is created against the shared layout
// (`vulkan-shared-layout.h`) and for dynamic rendering into `color_format`.
// Each returns VK_NULL_HANDLE on failure, with the reason in the log when
// it was the GLSL that failed.
// ---------------------------------------------------------------------------

/// Screen-space GUI quads: alpha blended, no depth attachment.
VkPipeline createVulkanGuiPipeline(VkDevice device, VkPipelineLayout layout,
                                   VkFormat color_format);

/// Static meshes: opaque, depth tested and written against D32.
VkPipeline createVulkanMeshPipeline(VkDevice device, VkPipelineLayout layout,
                                    VkFormat color_format);

/// Skinned meshes: the mesh fragment stage behind a skinning vertex stage,
/// with the static mesh's blend and depth state.
VkPipeline createVulkanSkinnedMeshPipeline(VkDevice device,
                                           VkPipelineLayout layout,
                                           VkFormat color_format);

/// The mesh outline: a full-screen triangle with no vertex input and no
/// depth attachment, blended over what the scene drew.
VkPipeline createVulkanOutlinePipeline(VkDevice device, VkPipelineLayout layout,
                                       VkFormat color_format);

/// Effects particles: `FxVertex` triangles already in clip space, no depth
/// attachment — the scene's depth is read as a texture — and premultiplied
/// blending.
VkPipeline createVulkanFxPipeline(VkDevice device, VkPipelineLayout layout,
                                  VkFormat color_format);

/// The volumetric-smoke pipeline: `FxVolumeVertex` triangles blended
/// premultiplied in the pass after the scene's, each fragment marching a
/// ray through noise against the depth it reads.
VkPipeline createVulkanFxVolumePipeline(VkDevice device,
                                        VkPipelineLayout layout,
                                        VkFormat color_format);

/// The water surface: `MeshVertex` triangles in the scene pass, blended
/// premultiplied over what the opaque meshes left and tested against their
/// depth without writing it.
VkPipeline createVulkanWaterPipeline(VkDevice device, VkPipelineLayout layout,
                                     VkFormat color_format);

}  // namespace eng::render

#endif  // ENGINE_RENDERER_VULKAN
