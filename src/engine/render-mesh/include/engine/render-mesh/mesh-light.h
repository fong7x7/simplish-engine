#pragma once

/// @file mesh-light.h
/// @brief One light the mesh shading model reads, in the layout it reads it.
/// @par Threading Thread-safe (immutable value type).

#include <cstddef>
#include <engine/math/vec3.h>

namespace eng {

/// Ambient floor: how lit a surface facing away from every light is.
///
/// This and `MESH_LIGHT_DIFFUSE` below are the split the editor has shaded
/// with since meshes first drew, kept exactly so that a scene with no
/// lights in it looks as it always did.
///
/// Every constant in this file is spelled out again in each backend's mesh
/// shader — `MESH_MSL_SOURCE` in `metal-device-impl.mm` and
/// `MESH_HLSL_SOURCE` in `dx12-builtin-pipelines.cpp` — neither of which
/// can include a C++ header. Changing one here means changing it in both,
/// and `MESH_MAX_LIGHTS` most of all: each shader's array is sized by its
/// own copy, so a smaller value here would leave it reading past the bytes
/// the draw actually sent.
inline constexpr float MESH_LIGHT_AMBIENT = 0.38f;

/// What one light of intensity 1, hitting a surface head on, adds to that
/// floor. A light left at its default therefore reproduces the built-in key
/// light rather than blowing the scene out.
inline constexpr float MESH_LIGHT_DIFFUSE = 0.62f;

/// `MeshLight::kind` for a light with no position and no falloff.
inline constexpr float MESH_LIGHT_DIRECTIONAL = 0.0f;

/// `MeshLight::kind` for a light that falls off to nothing at its range.
inline constexpr float MESH_LIGHT_POINT = 1.0f;

/// Direction the built-in key light arrives from.
///
/// Up, and over the viewer's left shoulder: the direction the editor's
/// camera implies, so a model's top face reads brightest.
inline constexpr Vec3 MESH_KEY_LIGHT_DIRECTION{-0.35f, -0.45f, 0.82f};

/// How many lights one draw is shaded by.
///
/// The editor's viewport budget, not the runtime's — [Engine REQUIREMENTS
/// §5.4] sizes the clustered path at 256, which is a light list in a buffer
/// rather than the handful of constant bytes a draw carries here. Lights
/// past this many are dropped, since a shader loop cannot grow. The shader
/// holds its own copy of this number; see the note above.
inline constexpr size_t MESH_MAX_LIGHTS = 8;

/// One light, laid out as the three `float4`s the shader reads.
///
/// Each vector is followed by the scalar that shares its register, which is
/// what makes the C++ record and the MSL one the same 48 bytes with no
/// padding word between them. Reordering these fields silently reinterprets
/// the light in the shader, so they move together or not at all.
/// @thread_safety Immutable value type.
struct alignas(16) MeshLight {
  /// World position a point light shines from. Ignored by a directional
  /// one, which arrives from the same direction everywhere.
  Vec3 position{};
  /// How far a point light reaches, in world units. Nothing beyond this is
  /// lit by it. Ignored by a directional light.
  float range = 0.0f;
  /// Unit vector from the lit surface towards the light. A zero vector
  /// lights nothing, which is what a light aimed nowhere should do.
  Vec3 direction = MESH_KEY_LIGHT_DIRECTION;
  /// Brightness multiplier, scaling this light's whole contribution.
  float intensity = 1.0f;
  /// Linear RGB tint, each component in [0, 1].
  Vec3 color{1.0f, 1.0f, 1.0f};
  /// `MESH_LIGHT_DIRECTIONAL` or `MESH_LIGHT_POINT`. A float rather than an
  /// enum because it is the fourth component of a register the shader reads
  /// as a `float4`.
  float kind = MESH_LIGHT_DIRECTIONAL;
};

// The shader reads this record as three `float4`s, which is only true while
// each vector and the scalar beside it share one 16-byte register. A field
// reordered or a type widened breaks that silently at runtime, and here.
static_assert(sizeof(MeshLight) == 48, "MeshLight must be three float4s");
static_assert(offsetof(MeshLight, range) == 12, "range shares position's w");
static_assert(offsetof(MeshLight, direction) == 16, "direction is register 1");
static_assert(offsetof(MeshLight, intensity) == 28,
              "intensity shares direction's w");
static_assert(offsetof(MeshLight, color) == 32, "colour is register 2");
static_assert(offsetof(MeshLight, kind) == 44, "kind shares colour's w");

}  // namespace eng
