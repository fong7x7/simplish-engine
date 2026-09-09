#pragma once

/// @file editor-placement-pick.h
/// @brief Which placement a click in the viewport lands on.
/// @par Threading Thread-safe (pure functions over value types).

#include <editor/shell/editor-placement-marker.h>
#include <editor/shell/editor-selection.h>
#include <editor/shell/iso-projection.h>
#include <optional>
#include <vector>

namespace eng::editor {

/// The direction the projection collapses points along, pointing toward the
/// camera.
///
/// The projection is oblique, so this is not the screen normal: it is the
/// ray derived in `iso-view-matrix.h`, four tiles along world Y for every
/// three up world Z. Every world point on one of these rays lands on the
/// same pixel, which is what makes it the ray to pick along.
inline constexpr float PICK_RAY_Y = ISO_TILE_RISE;
/// The ray's Z component. See `PICK_RAY_Y`.
inline constexpr float PICK_RAY_Z = ISO_TILE_DEPTH;

/// How far along the pick ray @p bounds is last met, or nothing when the
/// ray misses it.
///
/// The value increases toward the camera, so comparing it between two boxes
/// says which one is in front — the same ordering the depth buffer gives
/// the meshes themselves.
[[nodiscard]] std::optional<float>
placementRayHit(const IsoView& view, const PlacementBounds& bounds,
                IsoPoint screen);

/// Index of the marker under @p screen, or `EDITOR_PLACEMENT_NONE`.
///
/// Boxes rather than meshes: the editor does not keep mesh data on the CPU
/// after upload, and a box around a prop is what the viewport already
/// outlines — so what is picked is what is drawn. The cost is that a click
/// in the empty corner of a tall thin model still picks it, which is the
/// same bargain every bounds-picking editor makes.
///
/// Overlapping boxes are decided by depth, not by list order: the marker
/// nearest the camera along the pick ray wins, so clicking a prop standing
/// in front of another picks the one in front — the one whose mesh the
/// viewer can actually see.
[[nodiscard]] int
pickPlacementMarker(const IsoView& view,
                    const std::vector<EditorPlacementMarker>& markers,
                    IsoPoint screen);

}  // namespace eng::editor
