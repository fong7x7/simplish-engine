#pragma once

/// @file mesh-fragment-lights.h
/// @brief The block of lights a mesh fragment shader reads, in the layout
/// it reads it.
/// @par Threading Thread-safe (immutable value type and a pure function).

#include <cstddef>
#include <cstdint>
#include <engine/render-mesh/mesh-light.h>
#include <engine/render-mesh/mesh-style.h>
#include <span>

namespace eng {

/// What every mesh fragment stage reads at slot 0: the light count and the
/// band count, then the lights.
///
/// Shared by the static and the skinned mesh renderers, whose fragment
/// shaders are the same shader — a skinned mesh is lit exactly as a static
/// one is, and only its vertices move differently. A fixed array always
/// sent whole, so a draw with no lights needs no second code path in the
/// shader and no second binding.
/// @thread_safety Immutable value type.
struct alignas(16) MeshFragmentLights {
  /// How many entries of `lights` are live.
  uint32_t count = 0;
  /// Tones each light is flattened into; `MESH_SHADE_SMOOTH` for none.
  /// It rides in the count's register, which was padding before it.
  uint32_t shade_bands = MESH_SHADE_SMOOTH;
  /// The rest of that register. The array after it is read as `float4`s,
  /// which have to start on a register boundary.
  uint32_t padding[2]{};
  /// The lights, of which the first `count` are live.
  MeshLight lights[MESH_MAX_LIGHTS]{};
};

// Every shader's own struct puts the array one register in, and reads the
// band count as the second word of the first; padding that drifts here
// shifts every light the shader reads.
static_assert(offsetof(MeshFragmentLights, shade_bands) == 4,
              "the band count is the header's second word");
static_assert(offsetof(MeshFragmentLights, lights) == 16,
              "the lights follow the header's whole register");

/// The block for a draw lit by @p lights and flattened into @p shade_bands.
///
/// An empty list becomes the one default light, which is the built-in key
/// light — see `mesh-light.h`. Lights past `MESH_MAX_LIGHTS` are dropped:
/// the shader's loop is a fixed length and cannot grow.
[[nodiscard]] MeshFragmentLights
makeMeshFragmentLights(std::span<const MeshLight> lights, uint32_t shade_bands);

}  // namespace eng
