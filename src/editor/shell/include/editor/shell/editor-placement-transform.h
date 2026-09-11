#pragma once

/// @file editor-placement-transform.h
/// @brief Object-to-world transform and world bounds for a placed asset.
/// @par Threading Thread-safe (pure functions over value types).

#include <editor/shell/editor-asset.h>
#include <editor/shell/editor-placement-bounds.h>
#include <editor/shell/editor-placement.h>
#include <engine/math/mat4.h>

namespace eng::editor {

/// Sit an asset on a tile, scaled so its footprint fills one, turned by the
/// placement's rotation.
///
/// Models arrive in whatever unit their author used — a metre-scale crate
/// and a centimetre-scale one would otherwise differ by a hundred times.
/// Scaling by the larger horizontal extent puts every model at a readable
/// size on the grid while keeping its own proportions, and the vertical
/// offset drops its lowest point onto the ground plane rather than through
/// it.
///
/// The placement's own `scale` multiplies that fit, and like rotation it is
/// applied about the resting point, so a scaled prop still stands on its
/// tile.
///
/// Rotation is applied about that resting point — the centre of the
/// footprint at ground level — so turning a prop spins it where it stands
/// instead of swinging it away across the grid. The Euler angles are
/// applied X, then Y, then Z, which is the order the properties panel lists
/// them in.
///
/// The scale is uniform and rotation is orthonormal, so vertex normals stay
/// correct without a normal matrix — which is why `orientYUpToZUp` bakes
/// the axis change into the mesh instead of leaving it to this transform.
[[nodiscard]] Mat4 makePlacementTransform(const EditorAsset& asset,
                                          const EditorPlacement& placement);

/// The world-space box a placement occupies.
///
/// The asset's own corners are put through `makePlacementTransform` and the
/// box is taken around the result, so a rotated model reports the box that
/// contains it rather than its unrotated one.
///
/// An asset whose bounds were never measured — one whose mesh has not been
/// uploaded, which is every asset on a backend with no mesh pipeline —
/// reports the unit box standing on its tile. That keeps a placement
/// visible and clickable where it was dropped instead of collapsing it to a
/// point nobody can hit.
[[nodiscard]] PlacementBounds
placementWorldBounds(const EditorAsset& asset,
                     const EditorPlacement& placement);

}  // namespace eng::editor
