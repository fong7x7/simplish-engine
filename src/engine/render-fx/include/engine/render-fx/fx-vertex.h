#pragma once

/// @file fx-vertex.h
/// @brief One corner of a particle's quad, in the layout the effects
/// pipeline reads.
/// @par Threading
/// A value type.

#include <cstddef>

namespace eng {

/// A particle quad's corner, already projected: the CPU lays each quad out
/// facing the camera at one depth, so the vertex stage does nothing but
/// pass it on, and no backend's shader needs a matrix.
///
/// Every backend's effects pipeline declares this layout again —
/// `FX_MSL_SOURCE`, `FX_HLSL_SOURCE`, and the two GLSL copies — since none
/// of them can include this header.
struct FxVertex {
  /// Clip-space position.
  float clip[4]{};
  /// Premultiplied colour, as `FxColor`, already faded for its age.
  float color[4]{};
  /// Where in the quad this corner is, from -1 to 1 on each axis: the
  /// fragment stage fades the particle out towards the edge of that disc.
  float uv[2]{};
};

static_assert(sizeof(FxVertex) == 40, "the effects pipelines read 40 bytes");
static_assert(offsetof(FxVertex, color) == 16, "colour is the second float4");
static_assert(offsetof(FxVertex, uv) == 32, "uv follows the colour");

}  // namespace eng
