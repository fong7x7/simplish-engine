#pragma once

/// @file water-shading.h
/// @brief The block the water shader's fragment stage reads.
/// @par Threading A value type.

#include <cstddef>

namespace eng {

/// Fragment stage bytes at slot 0 of the water pipeline: a `WaterLook`, the
/// eye, and how much detail to draw, as float4s so every backend reads the
/// same bytes. The water's own colour, opacity and depth come per vertex;
/// the scene's lights are the second block, at slot 1, in the mesh shader's
/// own layout (`MeshFragmentLights`).
///
/// Every backend's water shader declares this layout again —
/// `WATER_MSL_SOURCE`, `WATER_PS_HLSL_SOURCE`, and the two GLSL copies —
/// since none of them can include this header.
struct WaterShading {
  /// What the surface reflects, sRGB; `w` unused.
  float sky[4]{};
  /// Foam's colour; `w` how much foam the crests throw, 0 for none.
  float foam[4]{};
  /// `x` the clearest water's absorption, `y` the murkiest's, `z` the tiles
  /// over which the colour darkens, `w` how much darker deep water gets.
  float clarity[4]{};
  /// `w` how bright the lights' glints off the water are; `xyz` unused.
  float light[4]{};
  /// Unit direction towards the eye; `w` the seconds the waves are at.
  float view[4]{};
  /// `x` 1 for wind waves finer than the simulation; `y` how strongly the
  /// ground under shallow water catches light; `z` 1 for any wind waves at
  /// all, 0 for a still surface; `w` unused.
  float detail[4]{};
};

static_assert(sizeof(WaterShading) == 96, "the water shaders read 96 bytes");
static_assert(offsetof(WaterShading, clarity) == 32, "the clarity is third");
static_assert(offsetof(WaterShading, view) == 64, "the eye is fifth");
static_assert(offsetof(WaterShading, detail) == 80, "the detail is last");

}  // namespace eng
