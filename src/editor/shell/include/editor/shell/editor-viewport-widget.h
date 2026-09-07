#pragma once

// Design Summary -- EditorViewportWidget
//
// Behaviours:
//   - Draws the dimetric level grid: tile lines, world origin axes, and a
//     hover highlight on the tile under the cursor
//   - The grid can be hidden without affecting axes, hover, or picking
//   - Outlines the footprint of every placement, so placed assets are
//     visible even on a backend with no mesh pipeline
//   - Left-drag or middle-drag pans the camera; the world tracks the
//     cursor one-to-one at any zoom
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
//   - The widget never fills its rect. 3D geometry is drawn in the scene
//     pass, which runs before the GUI pass, so an opaque background here
//     would erase it. The frame clear provides the background instead
//   - Grid drawing is clipped to the widget rect via the renderer scissor
//
// Integration Points:
//   - SimplishEditor: inserts this widget and owns its rect via layout
//   - EditorShellState: reads hoveredTile() for the status text

#include <editor/shell/iso-camera.h>
#include <editor/shell/iso-projection.h>
#include <engine/gui/gui-color.h>
#include <engine/gui/gui-widget.h>
#include <memory>
#include <vector>

namespace eng::editor {

/// Background behind the level.
///
/// The widget does not paint this: the frame clear does, so that the scene
/// pass can draw geometry the GUI pass will not erase. It lives here
/// because it is the viewport's colour, and the editor sets the frame clear
/// from it.
inline constexpr GuiColor EDITOR_VIEWPORT_BG{22, 22, 26, 255};

/// The level viewport: a dimetric tile grid with pan and zoom.
/// @thread_safety Main-thread only.
class EditorViewportWidget : public GuiWidget {
public:
  EditorViewportWidget();

  /// Polymorphic deep-copy.
  [[nodiscard]] std::unique_ptr<GuiWidget> clone() const override;

  /// Draw background, grid, origin axes, and the hover highlight.
  void render(const GuiDrawContext& ctx) const override;

  /// Begin a pan on left- or middle-drag. Returns true to capture.
  ///
  /// Left-drag pans, which means the viewport captures every left press.
  /// When the authoring tools start editing on click they will need a
  /// modifier, a mode, or a different button to share it with.
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

  /// Whether the tile grid is drawn. Hiding it leaves picking untouched —
  /// the grid is a drawing, not the source of tile coordinates.
  bool show_grid = true;

  /// Tiles holding a placed asset, outlined as footprints.
  ///
  /// The 3D meshes themselves are drawn in the scene pass, which the
  /// viewport widget has no part in — these outlines are the overlay that
  /// says where things are, and the only thing visible at all on a backend
  /// without a mesh pipeline.
  std::vector<WorldPoint> placement_markers{};

private:
  /// Draw the scissored world layer: grid, axes, placements, and hover.
  void renderScene(GuiRendererContext& renderer) const;

  /// Outline the footprint of every placement.
  void renderPlacements(GuiRendererContext& renderer,
                        const IsoView& view) const;

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
