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

/// How far along the pick ray @p bounds is last met, or nothing when the
/// ray misses it.
///
/// The ray is `isoProjectionRay(view.axes)`: every world point along it
/// lands on the clicked pixel, which is what makes it the one to pick
/// along, and which is why the view has to be the one that drew the
/// markers rather than any view of the same scene.
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
