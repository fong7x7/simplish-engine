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

/// Length of `isoProjectionRay`, used to normalise depth into tiles.
///
/// Not constexpr because it takes a square root, and not folded into the
/// ray itself because picking wants the raw direction: normalising there
/// would divide every slab test by the same number twice.
[[nodiscard]] float isoRayLength(const IsoAxes& axes);

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
/// The projection is oblique, not a rotation, in either of the projections
/// a project can choose (ADR-003). That has a consequence a normal renderer
/// does not have to think about — the direction along which points collapse
/// to one pixel is not the screen normal. `isoProjectionRay` derives it
/// from the axes, and depth is measured along it rather than along world Y.
///
/// Moving along that ray moves a point nearer the camera, so depth
/// decreases along it. A wall one tile nearer the viewer occludes what
/// stands behind it, and the top of a tall thing occludes what is behind
/// its base.
[[nodiscard]] Mat4 makeIsoViewProjection(const IsoView& view,
                                         const IsoViewTarget& target);

}  // namespace eng::editor
