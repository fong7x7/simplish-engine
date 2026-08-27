#pragma once

/// @file iso-projection.h
/// @brief 4:3 dimetric world-to-screen projection for the editor viewport.
/// @par Threading Thread-safe (pure functions over value types).

#include <engine/gui/gui-rect.h>

namespace eng::editor {

/// Width of one tile's screen footprint at zoom 1.0, in logical pixels.
///
/// The camera has zero yaw, so world +X runs straight right across the
/// screen and a tile occupies exactly `ISO_TILE_WIDTH` pixels horizontally.
inline constexpr float ISO_TILE_WIDTH = 64.0f;

/// Screen-space depth of one tile: how far +1 world Y moves down the screen.
///
/// Three quarters of the width — the 4:3 dimetric foreshortening fixed by
/// docs/decisions/ADR-003-hybrid-iso-render-model.md. Changing this changes
/// the projection every sprite is authored against, so it is a constant
/// rather than a setting.
inline constexpr float ISO_TILE_DEPTH = ISO_TILE_WIDTH * 0.75f;

/// Screen-space rise of one world height unit: how far +1 world Z moves up.
///
/// Equal to `ISO_TILE_WIDTH`, i.e. unforeshortened. The height axis is drawn
/// straight up the screen at the same scale as the horizontal axis, so
/// vertical surfaces are seen face-on rather than skewed. That equal scaling
/// of X and Z against a foreshortened Y is what makes the view dimetric.
inline constexpr float ISO_TILE_RISE = ISO_TILE_WIDTH;

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
[[nodiscard]] constexpr IsoPoint worldToIso(WorldPoint world) {
  return {world.x * ISO_TILE_WIDTH,
          world.y * ISO_TILE_DEPTH - world.z * ISO_TILE_RISE};
}

/// Invert `worldToIso` onto the ground plane, returning `z == 0`.
///
/// The inverse of a projection is only defined once a plane is chosen: a
/// screen point is both a ground tile far away and a raised tile nearer the
/// camera. Picking against a raised plane is the height tool's job.
[[nodiscard]] constexpr WorldPoint isoToWorld(IsoPoint iso) {
  return {iso.x / ISO_TILE_WIDTH, iso.y / ISO_TILE_DEPTH, 0.0f};
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
  return isoToScreen(view, worldToIso(world));
}

/// Map screen coordinates back to ground-plane world tile coordinates.
[[nodiscard]] constexpr WorldPoint screenToWorld(const IsoView& view,
                                                 IsoPoint screen) {
  return isoToWorld(screenToIso(view, screen));
}

}  // namespace eng::editor
