#pragma once

/// @file mesh-instance.h
/// @brief One placement of an uploaded mesh, with its world transform.

#include <cstdint>
#include <engine/math/mat4.h>

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
};

}  // namespace eng
