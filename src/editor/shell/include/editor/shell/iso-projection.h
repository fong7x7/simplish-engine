#pragma once

/// @file iso-projection.h
/// @brief World-to-screen projection for the editor viewport.
/// @par Threading Thread-safe (pure functions over value types).

#include <editor/shell/iso-axes.h>
#include <engine/gui/gui-rect.h>

namespace eng::editor {

/// A point on the isometric plane before camera and zoom are applied.
/// @thread_safety Immutable value type.
struct IsoPoint {
  /// Horizontal position on the isometric plane.
  float x = 0.0f;
  /// Vertical position on the isometric plane.
  float y = 0.0f;
};

/// A point in world tile space.
/// @thread_safety Immutable value type.
struct WorldPoint {
  /// World X in tiles (fractional within a tile).
  float x = 0.0f;
  /// World Y in tiles.
  float y = 0.0f;
  /// World height in tiles above the ground plane.
  float z = 0.0f;
};

/// Project world tile coordinates onto the isometric plane.
[[nodiscard]] constexpr IsoPoint worldToIso(const IsoAxes& axes,
                                            WorldPoint world) {
  return {world.x * axes.x_across + world.y * axes.y_across,
          world.x * axes.x_down + world.y * axes.y_down - world.z * axes.z_up};
}

/// Invert `worldToIso` onto the ground plane, returning `z == 0`.
///
/// The inverse of a projection is only defined once a plane is chosen: a
/// screen point is both a ground tile far away and a raised tile nearer the
/// camera. Picking against a raised plane is the height tool's job.
[[nodiscard]] constexpr WorldPoint isoToWorld(const IsoAxes& axes,
                                              IsoPoint iso) {
  // Inverting the 2x2 the ground axes form. Its determinant is non-zero for
  // any projection that shows the ground at all: a zero would mean both
  // world axes landing on one screen line.
  const float det = axes.x_across * axes.y_down - axes.y_across * axes.x_down;
  return {(iso.x * axes.y_down - iso.y * axes.y_across) / det,
          (iso.y * axes.x_across - iso.x * axes.x_down) / det, 0.0f};
}

/// The world direction the projection collapses points along, pointing
/// toward the camera.
///
/// The projection is oblique, so this is not the screen normal. Solving
/// `worldToIso(axes, v) == (0, 0)` for a non-zero `v` gives the null
/// direction below: every world point on one of these rays lands on the
/// same pixel. That makes it both the ray to pick along and the axis to
/// measure depth on. For the dimetric axes it comes out as four tiles along
/// Y for every three up Z; for the isometric ones, two along each ground
/// axis for every one up.
///
/// The length is arbitrary — only the direction means anything — so
/// callers that need a unit measure scale it themselves.
[[nodiscard]] constexpr WorldPoint isoProjectionRay(const IsoAxes& axes) {
  const float det = axes.x_across * axes.y_down - axes.y_across * axes.x_down;
  return {-axes.y_across, axes.x_across, det / axes.z_up};
}

/// Camera-and-viewport transform applied on top of `worldToIso`.
/// @thread_safety Immutable value type.
struct IsoView {
  /// The viewport rectangle in screen space.
  Rect viewport{};
  /// Isometric-plane point pinned to the viewport centre.
  IsoPoint focus{};
  /// Scale factor; 1.0 draws a tile at `ISO_TILE_WIDTH` pixels wide.
  float zoom = 1.0f;
  /// How the world axes land on the screen, from the project's projection.
  IsoAxes axes{};
};

/// Map an isometric-plane point to screen coordinates.
[[nodiscard]] constexpr IsoPoint isoToScreen(const IsoView& view,
                                             IsoPoint iso) {
  const float cx = view.viewport.x + view.viewport.w * 0.5f;
  const float cy = view.viewport.y + view.viewport.h * 0.5f;
  return {cx + (iso.x - view.focus.x) * view.zoom,
          cy + (iso.y - view.focus.y) * view.zoom};
}

/// Invert `isoToScreen`. Undefined when `view.zoom` is zero.
[[nodiscard]] constexpr IsoPoint screenToIso(const IsoView& view,
                                             IsoPoint screen) {
  const float cx = view.viewport.x + view.viewport.w * 0.5f;
  const float cy = view.viewport.y + view.viewport.h * 0.5f;
  return {(screen.x - cx) / view.zoom + view.focus.x,
          (screen.y - cy) / view.zoom + view.focus.y};
}

/// Map world tile coordinates straight to screen coordinates.
[[nodiscard]] constexpr IsoPoint worldToScreen(const IsoView& view,
                                               WorldPoint world) {
  return isoToScreen(view, worldToIso(view.axes, world));
}

/// Map screen coordinates back to ground-plane world tile coordinates.
[[nodiscard]] constexpr WorldPoint screenToWorld(const IsoView& view,
                                                 IsoPoint screen) {
  return isoToWorld(view.axes, screenToIso(view, screen));
}

}  // namespace eng::editor
