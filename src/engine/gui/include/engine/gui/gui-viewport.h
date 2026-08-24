#pragma once

/// @file gui-viewport.h
/// @brief 3D viewport component: grid, axes, orbit/pan/zoom.
/// @par Threading Main thread only.

#include "gui-viewport-camera.h"
#include "gui-widget.h"

namespace eng {

class GuiViewport : public GuiWidget {
public:
  /// Polymorphic deep-copy.
  std::unique_ptr<GuiWidget> clone() const override;

  /// Render the viewport background, grid, axes, and title.
  void render(const GuiDrawContext& ctx) const override;

  /// Start orbit or pan drag on mouse-down. Returns true to capture.
  bool handleMouseDown(const GuiMouseEvent& event) override;

  /// End drag on mouse-up.
  void handleMouseUp(const GuiMouseEvent& event) override;

  /// Update camera orbit or pan during drag.
  void handleMouseMove(const GuiMouseEvent& event) override;

  /// Zoom camera on scroll wheel. Returns true if consumed.
  bool handleScroll(const GuiScrollEvent& event) override;

  /// Camera orbit/pan state.
  ViewportCamera camera{};

private:
  /// Apply pan motion to camera target.
  void applyPan(float dx, float dy);
  /// Apply orbit rotation to camera yaw/pitch.
  void applyOrbit(float dx, float dy);

  /// Whether a drag operation is currently active.
  bool dragging_ = false;
  /// Whether the active drag is a pan (vs orbit).
  bool panning_ = false;
  /// Last mouse X during drag.
  float drag_last_x_ = 0.0f;
  /// Last mouse Y during drag.
  float drag_last_y_ = 0.0f;
};

}  // namespace eng
