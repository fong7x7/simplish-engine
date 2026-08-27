#pragma once

/// @file iso-camera.h
/// @brief Pan-and-zoom camera for the dimetric editor viewport.
/// @par Threading Main-thread-only.

#include <algorithm>
#include <editor/shell/iso-projection.h>

namespace eng::editor {

/// Smallest allowed zoom (tiles at 1/4 size).
inline constexpr float ISO_ZOOM_MIN = 0.25f;
/// Largest allowed zoom (tiles at 4x size).
inline constexpr float ISO_ZOOM_MAX = 4.0f;
/// Multiplier applied per scroll notch.
inline constexpr float ISO_ZOOM_STEP = 1.12f;

/// Editor viewport camera.
///
/// Pan and zoom only — there is deliberately no rotation. The projection is
/// fixed at zero yaw and 4:3 dimetric foreshortening, and the renderer is
/// allowed to depend on that
/// (docs/decisions/ADR-003-hybrid-iso-render-model.md). A rotating editor
/// camera would show the world at angles no sprite is authored for.
/// @thread_safety Main-thread-only.
struct IsoCamera {
  /// Isometric-plane point pinned to the viewport centre.
  IsoPoint focus{};
  /// Scale factor, clamped to [ISO_ZOOM_MIN, ISO_ZOOM_MAX].
  float zoom = 1.0f;
};

/// Pan by a screen-space delta, converting through the current zoom so the
/// world tracks the cursor one-to-one.
inline void panCamera(IsoCamera& camera, float screen_dx, float screen_dy) {
  camera.focus.x -= screen_dx / camera.zoom;
  camera.focus.y -= screen_dy / camera.zoom;
}

/// Apply @p notches of scroll zoom, keeping the world point under
/// (@p anchor_x, @p anchor_y) fixed on screen.
inline void zoomCameraAt(IsoCamera& camera, const Rect& viewport, float notches,
                         float anchor_x, float anchor_y) {
  const IsoView before{viewport, camera.focus, camera.zoom};
  const IsoPoint anchor_iso = screenToIso(before, {anchor_x, anchor_y});

  float scaled = camera.zoom;
  for (float i = 0; i < notches; i += 1.0f) {
    scaled *= ISO_ZOOM_STEP;
  }
  for (float i = 0; i > notches; i -= 1.0f) {
    scaled /= ISO_ZOOM_STEP;
  }
  camera.zoom = std::clamp(scaled, ISO_ZOOM_MIN, ISO_ZOOM_MAX);

  // Re-pin: shift focus so the anchored iso point lands where it started.
  const IsoView after{viewport, camera.focus, camera.zoom};
  const IsoPoint anchor_now = screenToIso(after, {anchor_x, anchor_y});
  camera.focus.x += anchor_iso.x - anchor_now.x;
  camera.focus.y += anchor_iso.y - anchor_now.y;
}

/// Build the projection view for a camera and viewport rect.
[[nodiscard]] inline IsoView makeIsoView(const IsoCamera& camera,
                                         const Rect& viewport) {
  return {viewport, camera.focus, camera.zoom};
}

}  // namespace eng::editor
