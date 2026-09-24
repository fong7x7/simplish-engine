#pragma once

/// @file water-vertex-uniforms.h
/// @brief The block the water shader's vertex stage reads.
/// @par Threading A value type.

#include <cstddef>
#include <engine/math/mat4.h>

namespace eng {

/// Vertex stage bytes at slot 1 of the water pipeline — the slot the mesh
/// pipeline reads its matrices from.
///
/// The surface is built in world coordinates, so there is no model matrix;
/// in its place is where the field's texture lies on the ground, which the
/// vertex stage turns a world position into a texture coordinate with.
struct WaterVertexUniforms {
  /// World-to-clip.
  Mat4 view_projection{};
  /// The field's south-west corner, in tiles, then one over its width and
  /// its height in tiles: `uv = (xy − field.xy) × field.zw`.
  float field[4]{};
};

static_assert(sizeof(WaterVertexUniforms) == 80,
              "the water shaders read 80 bytes");
static_assert(offsetof(WaterVertexUniforms, field) == 64,
              "the field follows the matrix");

}  // namespace eng
