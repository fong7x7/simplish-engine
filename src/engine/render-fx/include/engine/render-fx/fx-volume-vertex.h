#pragma once

/// @file fx-volume-vertex.h
/// @brief One corner of the screen rectangle a cloud of smoke is marched
/// through, in the layout the volume pipeline reads.
/// @par Threading
/// A value type.

#include <cstddef>

namespace eng {

/// A corner of the rectangle one cloud covers on screen, carrying
/// everything its fragments march with.
///
/// The camera is orthographic and never rotates, so the ray through every
/// pixel runs the same way and world position varies affinely with screen
/// position on a fixed plane. That is what lets the CPU hand the fragment
/// stage a ray by interpolating four corners, with no matrix in the
/// shader: `origin` is where this corner's ray crosses the plane through
/// the cloud's middle, and `ray` is the direction it carries on in.
///
/// Both are in the cloud's own space, where its box is -1 to 1 on each
/// axis, so the fragment stage's slab test and its noise field need no
/// extents of their own.
///
/// Every backend's volume pipeline declares this layout again —
/// `FX_VOLUME_MSL_SOURCE`, `FX_VOLUME_HLSL_SOURCE`, and the two GLSL
/// copies — since none of them can include this header.
struct FxVolumeVertex {
  /// Clip-space position of the corner, on the plane through the middle.
  float clip[4]{};
  /// What a full march of the smoke adds and hides, premultiplied; the
  /// fragment stage scales it by how much of the ray the smoke covered.
  float color[4]{};
  /// Where this corner's ray crosses that plane, in the cloud's space, and
  /// in `w` the seed its noise field is moved by.
  float origin[4]{};
  /// The way the ray carries on, in the cloud's space, per tile of world
  /// distance; and in `w` how much clip depth one tile of it is worth.
  float ray[4]{};
  /// Clip depth of the plane, then how thick the smoke is now. The last
  /// two are spare, and keep the stride a multiple of a float4.
  float params[4]{};
};

static_assert(sizeof(FxVolumeVertex) == 80,
              "the volume pipelines read 80 bytes");
static_assert(offsetof(FxVolumeVertex, color) == 16, "colour is second");
static_assert(offsetof(FxVolumeVertex, origin) == 32, "the origin is third");
static_assert(offsetof(FxVolumeVertex, ray) == 48, "the ray is fourth");
static_assert(offsetof(FxVolumeVertex, params) == 64, "the params are last");

}  // namespace eng
