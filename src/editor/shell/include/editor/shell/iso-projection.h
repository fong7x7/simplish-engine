#pragma once

/// @file iso-projection.h
/// @brief 2:1 dimetric world-to-screen projection for the editor viewport.
/// @par Threading Thread-safe (pure functions over value types).

#include <engine/gui/gui-rect.h>

namespace eng::editor {

/// Width of one tile's screen footprint at zoom 1.0, in logical pixels.
inline constexpr float ISO_TILE_WIDTH = 64.0f;
/// Height of one tile's screen footprint at zoom 1.0.
///
/// Exactly half the width: the 2:1 dimetric ratio fixed by
/// docs/decisions/ADR-003-hybrid-iso-render-model.md. Changing this changes
/// the projection every sprite is authored against, so it is a constant
/// rather than a setting.
inline constexpr float ISO_TILE_HEIGHT = ISO_TILE_WIDTH * 0.5f;

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
};

/// Project world tile coordinates onto the isometric plane.
[[nodiscard]] constexpr IsoPoint worldToIso(WorldPoint world) {
  return {(world.x - world.y) * (ISO_TILE_WIDTH * 0.5f),
          (world.x + world.y) * (ISO_TILE_HEIGHT * 0.5f)};
}

/// Invert `worldToIso`.
[[nodiscard]] constexpr WorldPoint isoToWorld(IsoPoint iso) {
  const float half_w = ISO_TILE_WIDTH * 0.5f;
  const float half_h = ISO_TILE_HEIGHT * 0.5f;
  const float a = iso.x / half_w;
  const float b = iso.y / half_h;
  return {(a + b) * 0.5f, (b - a) * 0.5f};
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

/// Map screen coordinates back to world tile coordinates.
[[nodiscard]] constexpr WorldPoint screenToWorld(const IsoView& view,
                                                 IsoPoint screen) {
  return isoToWorld(screenToIso(view, screen));
}

}  // namespace eng::editor
