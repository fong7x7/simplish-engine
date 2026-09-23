#pragma once

/// @file ground-mesh.h
/// @brief The geometry a painted ground grid is drawn with.
/// @par Threading Thread-safe (pure function over value types).

#include <cstdint>
#include <engine/render-ground/ground-grid.h>
#include <engine/render-mesh/mesh-data.h>

namespace eng {

/// How far above the one below it each layer is drawn, in tiles.
///
/// Every layer is flat, and two flat surfaces at one height fight for the
/// depth buffer. This is enough to settle that at any zoom the editor
/// allows and small enough that eight layers together stay well under the
/// thickness of the built-in tile shape, so a tile laid on painted ground
/// still sits on top of it.
inline constexpr float GROUND_LAYER_STEP = 1.0f / 512.0f;

/// How many straight segments approximate each rounded quarter.
inline constexpr uint32_t GROUND_ARC_SEGMENTS = 6;

/// How many texels wide and tall each layer's swatch of the atlas is.
inline constexpr uint32_t GROUND_SWATCH_TEXELS = 32;

/// The ground @p grid shows, one flat layer per terrain from 1 to
/// @p layer_count, each shaped a quarter-cell at a time by
/// `groundQuarterShape`.
///
/// Texture coordinates address an atlas one swatch wide and
/// @p layer_count swatches tall, layer 1 at the top: each cell maps its own
/// square onto its layer's swatch, inset half a texel so filtering never
/// reads the swatch beside it. A swatch that tiles therefore draws as one
/// continuous surface however the cells join. Normals are all +Z.
///
/// Empty when nothing is painted. In world coordinates: the caller draws it
/// with the identity transform.
[[nodiscard]] MeshData makeGroundMesh(const GroundGrid& grid,
                                      uint8_t layer_count);

}  // namespace eng
