#pragma once

/// @file mesh-vertex.h
/// @brief One vertex of a static mesh, in the layout uploaded to the GPU.

#include <engine/math/vec3.h>

namespace eng {

/// A mesh vertex: position and normal, tightly packed so a vector of these
/// can be uploaded as a vertex buffer without a repack.
/// @thread_safety Immutable value type.
struct MeshVertex {
  /// Object-space position.
  Vec3 position{};
  /// Unit normal, used for the editor's directional shading.
  Vec3 normal{};
};

}  // namespace eng
