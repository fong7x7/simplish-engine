#pragma once

#ifdef ENGINE_RENDERER_VULKAN

/// @file vulkan-glsl-compiler.h
/// @brief GLSL to SPIR-V, for the shaders the Vulkan backend ships itself.
/// @par Threading Main-thread-only, like device creation that calls it.

#include <cstdint>
#include <vector>
#include <vulkan/vulkan.h>

namespace eng::render {

/// Compile one stage of Vulkan-flavoured GLSL (`#version 450`) to SPIR-V
/// with glslang. Empty on failure, with glslang's log written to the
/// engine log — the counterpart of the `NSError` Metal hands back from
/// `newLibraryWithSource`, which that backend drops.
///
/// Only vertex and fragment stages are accepted; anything else compiles
/// as a vertex shader and fails on its first stage-specific line.
std::vector<uint32_t> compileVulkanGlsl(const char* source,
                                        VkShaderStageFlagBits stage);

}  // namespace eng::render

#endif  // ENGINE_RENDERER_VULKAN
