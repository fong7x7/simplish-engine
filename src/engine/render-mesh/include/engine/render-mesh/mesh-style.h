#pragma once

/// @file mesh-style.h
/// @brief How meshes look: smooth or banded light, and whether they are
/// outlined. Chosen per frame, so a game can change it while it runs.
/// @par Threading Thread-safe (immutable value type and pure functions).

#include <cstdint>
#include <engine/math/vec3.h>

namespace eng {

/// `MeshStyle::shade_bands` for light that falls off continuously.
inline constexpr uint32_t MESH_SHADE_SMOOTH = 0;

/// How bright a light leaves a surface once banding has had its say.
///
/// @p light is what one light contributes before its colour is applied —
/// lambert times falloff, in [0, 1]. With fewer than two bands it comes back
/// unchanged. Otherwise the range is cut into @p bands equal steps and each
/// step becomes one flat tone, from none of the light up to all of it: three
/// bands give an unlit side, a half-lit one, and a fully lit face.
///
/// Every mesh shader restates this — `mesh_band` in `MESH_MSL_SOURCE`,
/// `MESH_HLSL_SOURCE`, and `MESH_FRAGMENT_SHADER_GLSL` — because none of them
/// can include a C++ header. The CPU rasterizer calls it directly.
[[nodiscard]] constexpr float meshShadeBand(float light, uint32_t bands) {
  if (bands < 2) {
    return light;
  }
  if (light <= 0.0f) {
    return 0.0f;
  }
  // Truncation is floor here: the light is never negative by this point.
  const auto step = static_cast<uint32_t>(light * static_cast<float>(bands));
  const uint32_t top = bands - 1;
  return static_cast<float>(step < top ? step : top) / static_cast<float>(top);
}

/// The look of a mesh pass: the part of rendering a game changes at run
/// time to set its art style, and which has no effect on the simulation.
///
/// Two things, because cel shading is two things. Banding flattens each
/// light into a few tones inside the mesh shader; the outline is a second
/// pass over the depth the meshes wrote, drawing a line wherever it bends
/// sharply — along silhouettes, and along creases such as a box's edges.
/// @thread_safety Immutable value type.
struct MeshStyle {
  /// Tones each light is flattened into, or `MESH_SHADE_SMOOTH` for none.
  /// One band is the same as none: a single tone would be no light at all.
  uint32_t shade_bands = MESH_SHADE_SMOOTH;
  /// Outline thickness in layout pixels; zero draws no outline and skips
  /// the pass entirely.
  float outline_width = 0.0f;
  /// Outline colour, linear RGB with each component in [0, 1].
  Vec3 outline_color{};
};

/// Meshes as they have always drawn: smooth light, no outline.
inline constexpr MeshStyle MESH_STYLE_SMOOTH{};

/// Cel shading: three tones per light and a near-black line a pixel wide.
///
/// Three bands is the fewest that still reads as a lit form rather than a
/// silhouette with a highlight. The line is not pure black so that it sits
/// in the dark palette the game is drawn in rather than being punched out
/// of it.
inline constexpr MeshStyle MESH_STYLE_CEL{3, 1.0f, {0.012f, 0.010f, 0.016f}};

}  // namespace eng
