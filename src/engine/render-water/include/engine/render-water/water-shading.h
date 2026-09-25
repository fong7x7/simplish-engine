#pragma once

/// @file water-shading.h
/// @brief The block the water shader's fragment stage reads.
/// @par Threading A value type.

#include <cstddef>
#include <engine/math/mat4.h>

namespace eng {

/// Fragment stage bytes at slot 0 of the water pipeline: a `WaterLook`, the
/// eye, how much detail to draw, and where the scene it is drawn over lies
/// on the screen, as float4s so every backend reads the same bytes. The
/// water's own colour, opacity and depth come per vertex; the scene's
/// lights are the second block, at slot 1, in the mesh shader's own layout
/// (`MeshFragmentLights`).
///
/// Every backend's water shader declares this layout again —
/// `WATER_MSL_SOURCE`, `WATER_PS_HLSL_SOURCE`, and the two GLSL copies —
/// since none of them can include this header.
struct WaterShading {
  /// World-to-clip, the scene's: where a point the shader follows a ray
  /// to lands on the screen.
  Mat4 view_projection{};
  /// What the surface reflects where the scene does not, sRGB; `w` how
  /// much of the scene itself it reflects, 0 for none.
  float sky[4]{};
  /// Foam's colour; `w` how much foam the crests throw, 0 for none.
  float foam[4]{};
  /// `x` the clearest water's absorption, `y` the murkiest's, `z` the tiles
  /// over which the colour darkens, `w` how much darker deep water gets.
  float clarity[4]{};
  /// How much faster red, green and blue light are absorbed than the
  /// absorption says, in `xyz`; `w` how far, in tiles, a ripple bends the
  /// ground seen through it.
  float absorb[4]{};
  /// `x` how much darker wet ground at the shore is; `y` how high the
  /// waves lapping at the shore run; `z` how sharp the wind waves' crests
  /// are, 0 for sine waves; `w` how bright the lights' glints are.
  float light[4]{};
  /// Unit direction towards the eye; `w` the seconds the waves are at.
  float view[4]{};
  /// `x` 1 for wind waves finer than the simulation; `y` how strongly the
  /// ground under shallow water catches light; `z` 1 for any wind waves at
  /// all, 0 for a still surface; `w` 1 for a surface that runs with the
  /// water's flow, 0 for one that stands still whatever it holds.
  float detail[4]{};
  /// The viewport's corner in pixels, then one over the scene copy's width
  /// and height in pixels.
  float screen[4]{};
  /// The viewport's size in pixels; `z` the water's height in the world;
  /// `w` how far, in tiles, a reflection is looked for.
  float surface[4]{};
  /// One over the field's width and height in samples; `z` the width of a
  /// sample in tiles; `w` a pixel's, the finest detail worth drawing.
  float texel[4]{};
  /// `x` 1 to look for what stands in the water and ring its foot with
  /// foam, 0 not to; `yzw` unused.
  float toggles[4]{};
};

static_assert(sizeof(WaterShading) == 240, "the water shaders read 240 bytes");
static_assert(offsetof(WaterShading, sky) == 64, "the sky follows the matrix");
static_assert(offsetof(WaterShading, absorb) == 112, "the absorption is 4th");
static_assert(offsetof(WaterShading, view) == 144, "the eye is sixth");
static_assert(offsetof(WaterShading, screen) == 176, "the screen is eighth");
static_assert(offsetof(WaterShading, texel) == 208, "the texel size is 11th");
static_assert(offsetof(WaterShading, toggles) == 224, "the toggles are last");

}  // namespace eng
