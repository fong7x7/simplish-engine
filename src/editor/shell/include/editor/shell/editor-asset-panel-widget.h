#pragma once

// Design Summary -- EditorAssetPanelWidget
//
// Behaviours:
//   - Strip along the bottom of the window listing the project's assets as
//     named cards
//   - Pressing a card starts a drag; the card follows the cursor as a ghost
//   - Releasing outside the panel raises on_asset_dropped with the cursor
//     position, which the editor turns into a placement
//   - Releasing inside the panel cancels, so a mis-drag costs nothing
//
// Edge Cases:
//   - No project open, or no assets: the panel draws its empty-state line
//     and presses do nothing
//   - More cards than fit: the overflow is clipped. There is no scrolling
//     yet, so a large asset directory is only partly reachable
//   - The widget captures the mouse for the whole drag, so the drop point
//     may be anywhere on screen — the editor decides whether it is over the
//     viewport
//
// Invariants:
//   - Card hit testing and card drawing derive from the same layout maths,
//     so what is drawn is what is pressed
//   - A drag is only ever reported once, on release
//
// Integration Points:
//   - SimplishEditor: owns this widget, feeds it asset names, and handles
//     on_asset_dropped

#include <cstddef>
#include <engine/gui/gui-panel.h>
#include <engine/gui/gui-rect.h>
#include <functional>
#include <memory>
#include <string>
#include <vector>

namespace eng::editor {

/// Height of the asset strip in logical pixels.
inline constexpr float ASSET_PANEL_HEIGHT = 116.0f;

/// The bottom asset strip: named cards that can be dragged into the world.
/// @thread_safety Main-thread only.
class EditorAssetPanelWidget : public GuiPanel {
public:
  EditorAssetPanelWidget();

  /// Polymorphic deep-copy.
  [[nodiscard]] std::unique_ptr<GuiWidget> clone() const override;

  /// Draw the strip, its cards, and any in-flight drag ghost.
  void render(const GuiDrawContext& ctx) const override;

  /// Begin a drag when a card is pressed. Returns true to capture.
  bool handleMouseDown(const GuiMouseEvent& event) override;

  /// Track the cursor during a drag.
  void handleMouseMove(const GuiMouseEvent& event) override;

  /// Finish a drag, reporting a drop that landed outside the panel.
  void handleMouseUp(const GuiMouseEvent& event) override;

  /// Index of the card under a point, or -1.
  [[nodiscard]] int hitTestCard(float x, float y) const;

  /// Rect of the card at @p index, in layout pixels.
  [[nodiscard]] Rect cardRect(size_t index) const;

  /// Replace the listed assets.
  void setAssetNames(std::vector<std::string> names);

  /// Number of cards listed.
  [[nodiscard]] size_t assetCount() const { return names_.size(); }

  /// Index of the card being dragged, or -1 when no drag is in flight.
  [[nodiscard]] int draggingIndex() const { return dragging_; }

  /// Raised on release outside the panel, with the asset index and the
  /// cursor position in layout pixels.
  std::function<void(size_t, float, float)> on_asset_dropped{};

private:
  /// Draw the panel heading and, when there is nothing to list, why.
  void renderHeader(const GuiDrawContext& ctx) const;
  /// Draw every card that starts inside the panel.
  void renderCards(const GuiDrawContext& ctx) const;
  /// Draw one card's frame and label.
  void renderCard(const GuiDrawContext& ctx, size_t index) const;
  /// Draw the ghost that follows the cursor mid-drag.
  void renderDragGhost(const GuiDrawContext& ctx) const;

  /// Asset display names, in panel order.
  std::vector<std::string> names_{};
  /// Card being dragged, or -1.
  int dragging_ = -1;
  /// Last cursor X seen during a drag.
  float drag_x_ = 0.0f;
  /// Last cursor Y seen during a drag.
  float drag_y_ = 0.0f;
};

}  // namespace eng::editor
