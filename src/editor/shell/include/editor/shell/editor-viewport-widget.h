#pragma once

// Design Summary -- EditorViewportWidget
//
// Behaviours:
//   - Draws the dimetric level grid: tile lines, world origin axes, and a
//     hover highlight on the tile under the cursor
//   - Middle-drag, or left-drag with Shift, pans the camera
//   - Scroll wheel zooms about the cursor
//   - Reports the hovered tile so the toolbar/status text can show it
//
// Edge Cases:
//   - Zero-area rect: render and hit test are no-ops
//   - Scroll at the zoom clamp: camera is unchanged, event still consumed
//   - Pointer outside the rect: hover tile is not updated
//
// Invariants:
//   - The camera never rotates (ADR-003); only focus and zoom change
//   - Grid drawing is clipped to the widget rect via the renderer scissor
//
// Integration Points:
//   - SimplishEditor: inserts this widget and owns its rect via layout
//   - EditorShellState: reads hoveredTile() for the status text

#include <editor/shell/iso-camera.h>
#include <editor/shell/iso-projection.h>
#include <engine/gui/gui-widget.h>
#include <memory>

namespace eng::editor {

/// The level viewport: a dimetric tile grid with pan and zoom.
/// @thread_safety Main-thread only.
class EditorViewportWidget : public GuiWidget {
public:
  EditorViewportWidget();

  /// Polymorphic deep-copy.
  [[nodiscard]] std::unique_ptr<GuiWidget> clone() const override;

  /// Draw background, grid, origin axes, and the hover highlight.
  void render(const GuiDrawContext& ctx) const override;

  /// Begin a pan on middle-drag or Shift+left-drag. Returns true to capture.
  bool handleMouseDown(const GuiMouseEvent& event) override;

  /// End the active pan.
  void handleMouseUp(const GuiMouseEvent& event) override;

  /// Pan while dragging; otherwise update the hovered tile.
  void handleMouseMove(const GuiMouseEvent& event) override;

  /// Zoom about the cursor. Returns true — the viewport always consumes
  /// scroll so it never bubbles up and scrolls a parent container.
  bool handleScroll(const GuiScrollEvent& event) override;

  /// World tile under the cursor, floored to integer tile coordinates.
  [[nodiscard]] WorldPoint hoveredTile() const { return hovered_tile_; }

  /// Whether the cursor is currently over the viewport.
  [[nodiscard]] bool hasHover() const { return has_hover_; }

  /// Camera state; mutable so the shell can reset or frame the view.
  IsoCamera camera{};

private:
  /// Recompute `hovered_tile_` from a screen position.
  void updateHover(float x, float y);

  /// Tile under the cursor when `has_hover_` is true.
  WorldPoint hovered_tile_{};
  /// Whether the cursor is inside the viewport rect.
  bool has_hover_ = false;
  /// Whether a pan drag is active.
  bool panning_ = false;
  /// Last cursor X seen during a pan.
  float drag_last_x_ = 0.0f;
  /// Last cursor Y seen during a pan.
  float drag_last_y_ = 0.0f;
};

}  // namespace eng::editor
