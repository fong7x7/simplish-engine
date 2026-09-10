#pragma once

/// @file mesh-draw-bindings.h
/// @brief Where the mesh shaders read what each draw hands them: the slot
/// numbers, and the vertex stage's matrix block.
/// @par Threading Thread-safe (constants and an immutable value type).
///
/// Shared by the static and skinned renderers because their shaders share
/// every binding but one — the skinned vertex stage also reads a palette.
/// Each backend's shaders hard-code these slots.

#include <cstdint>
#include <engine/math/mat4.h>

namespace eng {

/// Vertex-stage slot the mesh shaders read their matrices from.
inline constexpr uint32_t MESH_UNIFORM_SLOT = 1;

/// Vertex-stage slot the skinned mesh shader reads its `SkinPalette` from.
/// Slot 0 is where Metal binds the vertex buffer itself, and 1 is the
/// matrices, so the palette takes the next one on every backend.
inline constexpr uint32_t MESH_SKIN_PALETTE_SLOT = 2;

/// Fragment-stage slot the mesh shaders read their lights from.
inline constexpr uint32_t MESH_LIGHT_SLOT = 0;

/// Fragment-stage slot the mesh shaders sample their diffuse map from.
inline constexpr uint32_t MESH_TEXTURE_SLOT = 0;

/// What the vertex stage reads at `MESH_UNIFORM_SLOT`.
///
/// Both matrices rather than their product: the fragment stage shades in
/// world space, so it needs the world position and the world normal, and
/// the vertex stage cannot recover either from a combined matrix.
/// @thread_safety Immutable value type.
struct MeshVertexUniforms {
  /// World-to-clip, shared by every instance of the draw.
  Mat4 view_projection{};
  /// Object-to-world for this instance.
  Mat4 model{};
};

}  // namespace eng
