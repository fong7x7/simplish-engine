#pragma once

/// @file mesh-vertex.h
/// @brief One vertex of a static mesh, in the layout uploaded to the GPU.

#include <cstddef>
#include <engine/math/vec2.h>
#include <engine/math/vec3.h>

namespace eng {

/// A mesh vertex: position, normal, and texture coordinate, tightly packed
/// so a vector of these can be uploaded as a vertex buffer without a
/// repack.
///
/// The backend's vertex descriptor restates this layout — see
/// `MESH_VERTEX_STRIDE` — so a field added here has to be added there too,
/// and `MESH_VERTEX_BYTES` below is what fails the build when they drift.
/// @thread_safety Immutable value type.
struct MeshVertex {
  /// Object-space position.
  Vec3 position{};
  /// Unit normal, used for the editor's directional shading.
  Vec3 normal{};
  /// Texture coordinate, origin at the top-left of the image.
  ///
  /// Zero for geometry that carries none — a model with no `vt` data, and
  /// every built-in shape — which samples one texel of the renderer's white
  /// fallback texture and so shades exactly as it did before textures.
  Vec2 uv{};
};

/// Size of one vertex as the GPU reads it.
///
/// Named here so a backend's vertex descriptor can assert against it rather
/// than restating a number that has moved.
inline constexpr size_t MESH_VERTEX_BYTES = sizeof(MeshVertex);

}  // namespace eng
