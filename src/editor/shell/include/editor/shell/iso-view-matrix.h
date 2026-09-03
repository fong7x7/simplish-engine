#pragma once

/// @file iso-view-matrix.h
/// @brief World-to-clip matrix for drawing 3D meshes in the editor viewport.
/// @par Threading Thread-safe (pure functions over value types).

#include <editor/shell/iso-projection.h>
#include <engine/math/mat4.h>

namespace eng::editor {

/// Half the clip-space depth range a scene may span, in tiles along the
/// view ray. Geometry past this clips; ±1024 tiles is far more level than
/// the editor will ever show at once.
inline constexpr float ISO_DEPTH_RANGE = 2048.0f;

/// Length of the projection ray's (Y, Z) components, used to normalise
/// depth into tiles. See `makeIsoViewProjection` for why those are the
/// components that matter.
inline constexpr float ISO_RAY_LENGTH = 80.0f;  // hypot(48, 64)

/// The layout-space rectangle a mesh pass draws into, and the layout size
/// clip space is measured against.
/// @thread_safety Immutable value type.
struct IsoViewTarget {
  /// Viewport rect in layout pixels.
  Rect viewport{};
  /// Full layout width, which NDC x spans.
  float layout_width = 1.0f;
  /// Full layout height, which NDC y spans.
  float layout_height = 1.0f;
};

/// Build the world-to-clip matrix that puts a mesh exactly where
/// `worldToScreen` puts the same point.
///
/// The projection is oblique, not a rotation: X and Z are unforeshortened
/// while Y is at 3/4 (ADR-003). That has a consequence a normal renderer
/// does not have to think about — the direction along which points collapse
/// to one pixel is not the screen normal. Solving `x*W = 0` and
/// `y*D - z*R = 0` gives that ray as `(0, R, D)` — four tiles along Y for
/// every three along Z — so depth has to be measured along it rather than
/// along world Y.
///
/// Both +Y (down the screen, toward the viewer, as in every 2.5D game) and
/// +Z (up) move a point nearer the camera, so depth decreases along
/// `y*R + z*D`. A wall one tile nearer the viewer occludes what stands
/// behind it, and the top of a tall thing occludes what is behind its base.
[[nodiscard]] Mat4 makeIsoViewProjection(const IsoView& view,
                                         const IsoViewTarget& target);

}  // namespace eng::editor
