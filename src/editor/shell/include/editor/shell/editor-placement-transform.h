#pragma once

/// @file editor-placement-transform.h
/// @brief Object-to-world transform for a placed asset.
/// @par Threading Thread-safe (pure function over value types).

#include <editor/shell/editor-asset.h>
#include <editor/shell/iso-projection.h>
#include <engine/math/mat4.h>

namespace eng::editor {

/// Sit an asset on a tile, scaled so its footprint fills one.
///
/// Models arrive in whatever unit their author used — a metre-scale crate
/// and a centimetre-scale one would otherwise differ by a hundred times.
/// Scaling by the larger horizontal extent puts every model at a readable
/// size on the grid while keeping its own proportions, and the vertical
/// offset drops its lowest point onto the ground plane rather than through
/// it.
///
/// The scale is uniform, so vertex normals stay correct without a normal
/// matrix — which is why `orientYUpToZUp` bakes the axis change into the
/// mesh instead of leaving it to this transform.
[[nodiscard]] Mat4 makePlacementTransform(const EditorAsset& asset,
                                          WorldPoint position);

}  // namespace eng::editor
