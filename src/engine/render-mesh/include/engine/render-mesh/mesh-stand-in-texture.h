#pragma once

/// @file mesh-stand-in-texture.h
/// @brief The one-texel texture a mesh naming no diffuse map is drawn with.
/// @par Threading Main-thread only (creates a GPU resource).

#include <cstdint>
#include <engine/render/rhi-core-types.h>
#include <engine/render/rhi-device.h>

namespace eng {

/// The colour an untextured mesh is shaded with, as one sRGB texel.
///
/// This is the constant the mesh shader used to carry, moved into a texture
/// so that textured and untextured meshes take the same path through it: an
/// instance naming no map samples this instead of branching. Sampling it
/// gives back what the old constant was to within a 255th, which is why
/// nothing already on screen changed when textures arrived.
inline constexpr uint8_t MESH_UNTEXTURED_TEXEL[4] = {189, 194, 204, 255};

/// Create a 1×1 texture holding `MESH_UNTEXTURED_TEXEL`. Invalid when the
/// device cannot create one.
///
/// Unorm, not sRGB: every mesh shader converts to linear itself, on the way
/// out and after the lighting. A texture the GPU decoded on sample would be
/// converted twice.
[[nodiscard]] RhiTextureHandle createMeshStandInTexture(RhiDevice& device);

}  // namespace eng
