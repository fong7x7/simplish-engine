#pragma once

/// @file mesh-instance.h
/// @brief One placement of an uploaded mesh, with its world transform.

#include <cstdint>
#include <engine/math/mat4.h>
#include <engine/render/rhi-core-types.h>

namespace eng {

/// Identifier for a mesh uploaded to the GPU. Zero is never valid.
using MeshGpuId = uint32_t;

/// Sentinel for "no mesh".
inline constexpr MeshGpuId MESH_GPU_INVALID = 0;

/// A mesh to draw and where to draw it.
/// @thread_safety Immutable value type.
struct MeshInstance {
  /// Which uploaded mesh to draw.
  MeshGpuId mesh = MESH_GPU_INVALID;
  /// Object-to-world transform, applied before the view projection.
  Mat4 model{};
  /// Diffuse map to shade with, or invalid to take the renderer's own
  /// untextured stand-in — see `MeshRenderer`.
  ///
  /// Held per instance rather than per mesh so the same geometry can be
  /// drawn with two different maps, and because the renderer does not own
  /// the texture: whoever loaded the image owns it and outlives the draw.
  RhiTextureHandle texture = RHI_TEXTURE_INVALID;
};

}  // namespace eng
