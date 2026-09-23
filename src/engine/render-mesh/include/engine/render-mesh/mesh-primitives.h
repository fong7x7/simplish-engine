#pragma once

/// @file mesh-primitives.h
/// @brief The simple shapes an editor can place without a file behind them.
/// @par Threading Thread-safe (pure functions producing new meshes).

#include <cstdint>
#include <engine/render-mesh/mesh-data.h>

namespace eng {

/// How many segments go around a shape with a curved surface.
///
/// Twenty-four reads as round at the size a prop is drawn on the grid,
/// without spending vertices on a silhouette nobody can resolve. It is the
/// same count for every curved shape so that two of them standing side by
/// side are faceted alike.
inline constexpr uint32_t MESH_PRIMITIVE_SEGMENTS = 24;

/// How many rings a sphere is divided into from pole to pole. Half the
/// segment count, which is what keeps its quads roughly square.
inline constexpr uint32_t MESH_PRIMITIVE_RINGS = 12;

/// How thick a tile is, as a fraction of its width.
///
/// Thick enough to lie clear of the ground plane rather than fight it for
/// the depth buffer, and to show a sliver of edge at the iso angle so a
/// tile reads as a thing laid down rather than a hole in the grid; thin
/// enough that nothing standing on it looks raised.
inline constexpr float MESH_PRIMITIVE_TILE_THICKNESS = 1.0f / 32.0f;

/// A cube standing on the ground plane.
///
/// Every shape here is built in the same unit box: one across in X and Y,
/// centred on the origin, and standing from z = 0 to z = 1. The editor's
/// placement transform scales whatever it is given to a tile footprint and
/// sits its lowest point on the ground, so a shape built this way arrives
/// exactly one tile across and needs no orientation pass — unlike a loaded
/// OBJ, which is Y-up until `orientYUpToZUp` turns it.
[[nodiscard]] MeshData makeCubeMesh();

/// A sphere filling that box, smooth-shaded: every normal is the direction
/// the point sits from the centre, so the facets read as a curve rather
/// than as panels.
[[nodiscard]] MeshData makeSphereMesh();

/// A square pyramid: the box's base, and an apex over its centre.
[[nodiscard]] MeshData makePyramidMesh();

/// A cylinder filling that box, with a smooth side and flat caps.
[[nodiscard]] MeshData makeCylinderMesh();

/// A flat slab lying on the ground: the box's full footprint, but only
/// `MESH_PRIMITIVE_TILE_THICKNESS` tall — the one shape that does not fill
/// the box's height. The placement transform scales by footprint alone, so
/// it still arrives exactly one tile across, and stays flat.
[[nodiscard]] MeshData makeTileMesh();

}  // namespace eng
