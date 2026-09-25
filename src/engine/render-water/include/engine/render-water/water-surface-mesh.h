#pragma once

/// @file water-surface-mesh.h
/// @brief The geometry the surface of painted water is drawn on.
/// @par Threading Thread-safe (pure function over value types).

#include <engine/render-ground/ground-mesh.h>
#include <engine/render-mesh/mesh-data.h>
#include <engine/render-water/water-layer.h>

namespace eng {

/// How high the water's surface is drawn, in tiles: over every layer the
/// ground can stack — ten layer steps, where the editor's palette uses six
/// — so water lies on top of whatever terrain it was painted over, and
/// still under the thickness of the built-in tile shape, so a tile standing
/// in water still stands out of it.
inline constexpr float WATER_SURFACE_HEIGHT = 10.0f * GROUND_LAYER_STEP;

/// The surface over every cell of @p layer's water, shaped by the same
/// quarter-cell rule as the ground and flat at `WATER_SURFACE_HEIGHT` —
/// after a wet band over every cell within one of it, drawn first so the
/// water goes over it. The band's vertices carry nothing, depth 0 among
/// it, which is how the shader tells the two apart; it darkens only the
/// ground the water has wet (`WATER_WET_TILES`).
///
/// Each vertex of the water itself carries the water there, blended between its
/// cells' corners (`waterSampleAt`) so the rasterizer carries it smoothly from
/// one tile to the next: `uv.x` the depth in tiles — not yet shelved towards
/// the bank, which the shader does from the field — `uv.y` the opacity,
/// and `normal` the colour, sRGB from 0 to 1, since the surface's normal is
/// +Z everywhere and the shader works its own out from the ripples. Empty
/// when there is no water. In world coordinates.
[[nodiscard]] MeshData makeWaterSurfaceMesh(const WaterLayer& layer);

}  // namespace eng
