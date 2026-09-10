#pragma once

/// @file skinned-mesh-vertex.h
/// @brief One vertex of a skinned mesh: a static vertex plus the joints it
/// hangs from, in the layout uploaded to the GPU.

#include <cstddef>
#include <cstdint>
#include <engine/math/vec2.h>
#include <engine/math/vec3.h>

namespace eng {

/// How many joints one vertex may follow. Four is what glTF's `JOINTS_0`
/// carries and what every exporter targets; a fifth influence is below
/// what anyone can see at the engine's camera distance.
inline constexpr size_t MESH_SKIN_INFLUENCES = 4;

/// A skinned mesh vertex: the three attributes a static vertex has, in the
/// same places, followed by four joint indices and their weights.
///
/// Joint indices are bytes because a skin's palette is capped well below
/// 256 — see `MESH_MAX_SKIN_JOINTS` — so a wider type would be bytes the GPU
/// fetches for every vertex and never uses.
///
/// Each backend's skinned vertex descriptor restates this layout. The
/// offsets below are what those descriptors assert against.
/// @thread_safety Immutable value type.
struct SkinnedMeshVertex {
  /// Object-space position in the bind pose.
  Vec3 position{};
  /// Unit normal in the bind pose.
  Vec3 normal{};
  /// Texture coordinate, origin at the top-left of the image.
  Vec2 uv{};
  /// Palette entries this vertex follows — indices into `Skin::joints`,
  /// not into the skeleton.
  uint8_t joints[MESH_SKIN_INFLUENCES]{};
  /// How much each of those entries moves the vertex. They sum to one, so
  /// a vertex in the bind pose stays exactly where it was modelled.
  float weights[MESH_SKIN_INFLUENCES]{1.0f, 0.0f, 0.0f, 0.0f};
};

/// Size of one skinned vertex as the GPU reads it.
inline constexpr size_t SKINNED_MESH_VERTEX_BYTES = sizeof(SkinnedMeshVertex);

/// Where the joint indices start within a skinned vertex.
inline constexpr size_t SKINNED_MESH_JOINTS_OFFSET =
    offsetof(SkinnedMeshVertex, joints);

/// Where the weights start within a skinned vertex.
inline constexpr size_t SKINNED_MESH_WEIGHTS_OFFSET =
    offsetof(SkinnedMeshVertex, weights);

// The backends hard-code these three numbers in their vertex descriptors,
// and the first 32 bytes have to match `MeshVertex` so the attributes both
// shaders share sit at the same offsets.
static_assert(SKINNED_MESH_JOINTS_OFFSET == 32, "joints follow the uv");
static_assert(SKINNED_MESH_WEIGHTS_OFFSET == 36, "weights follow the joints");
static_assert(SKINNED_MESH_VERTEX_BYTES == 52, "no padding in the vertex");

}  // namespace eng
