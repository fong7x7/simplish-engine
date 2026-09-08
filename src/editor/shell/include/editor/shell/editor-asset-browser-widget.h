#pragma once

// Design Summary -- EditorAssetBrowserWidget
//
// Behaviours:
//   - Strip along the bottom of the window, split into a folder pane on the
//     left and a card grid on the right
//   - The pane lists the project's asset folders as an indented tree; the
//     chevron opens and closes one, and the row selects it
//   - The grid shows the selected folder's assets as named cards, wrapped
//     into rows and scrolled when there are more than fit
//   - Pressing a card starts a drag; the card follows the cursor as a ghost
//   - Releasing outside the panel raises on_asset_dropped with the cursor
//     position, which the editor turns into a placement
//   - Releasing inside the panel cancels, so a mis-drag costs nothing
//
// Edge Cases:
//   - No project open, or no assets: the grid draws its empty-state line and
//     the pane lists the root alone
//   - A folder holding only other folders selects fine and shows an empty
//     grid, which is the honest answer for it
//   - Panel too narrow for the folder pane: the pane takes what there is and
//     the grid gets nothing, rather than either being handed a negative width
//   - The widget captures the mouse for the whole drag, so the drop point may
//     be anywhere on screen — the editor decides whether it is over the
//     viewport
//
// Invariants:
//   - Hit testing and drawing derive from the same layout maths, so what is
//     drawn is what is pressed. The scroll offsets are part of that maths,
//     so a scrolled row is still pressed where it appears
//   - `on_asset_dropped` reports an index into the whole asset list, not the
//     grid's own numbering, so what the editor places does not depend on
//     which folder happened to be selected
//   - A drag is only ever reported once, on release
//   - The selected folder is always a folder that exists
//
// Integration Points:
//   - SimplishEditor: owns this widget, feeds it the asset tree and names,
//     and handles on_asset_dropped

#include <cstddef>
#include <editor/shell/editor-asset-browser-layout.h>
#include <editor/shell/editor-asset-folder-rows.h>
#include <editor/shell/editor-asset-tree.h>
#include <engine/gui/gui-panel.h>
#include <engine/gui/gui-rect.h>
#include <functional>
#include <memory>
#include <string>
#include <unordered_set>
#include <vector>

namespace eng::editor {

/// The bottom asset browser: a folder pane and a grid of draggable cards.
/// @thread_safety Main-thread only.
class EditorAssetBrowserWidget : public GuiPanel {
public:
  EditorAssetBrowserWidget();

  /// Polymorphic deep-copy.
  [[nodiscard]] std::unique_ptr<GuiWidget> clone() const override;

  /// Draw the panel, its folder pane, its cards, and any drag ghost.
  void render(const GuiDrawContext& ctx) const override;

  /// Open or close a folder, or start a drag. Returns true to capture.
  bool handleMouseDown(const GuiMouseEvent& event) override;

  /// Track the cursor during a drag.
  void handleMouseMove(const GuiMouseEvent& event) override;

  /// Finish a drag, reporting a drop that landed outside the panel.
  void handleMouseUp(const GuiMouseEvent& event) override;

  /// Scroll whichever pane the cursor is over.
  bool handleScroll(const GuiScrollEvent& event) override;

  /// Replace what the browser lists. Names are indexed by the same numbers
  /// the tree's folders hold, which is the editor's whole asset list.
  void setAssets(EditorAssetTree tree, std::vector<std::string> names);

  /// The panel's regions for its current rect and fold state.
  [[nodiscard]] EditorAssetBrowserLayout layout() const;

  /// Height the panel wants from the editor's layout: its full height, or
  /// its header alone when folded.
  [[nodiscard]] float preferredHeight() const;

  /// Whether the folder pane is folded away.
  [[nodiscard]] bool navCollapsed() const { return nav_collapsed_; }

  /// Fold the folder pane away, giving the cards its width.
  void hideFolderPane();

  /// Bring the folder pane back.
  void showFolderPane();

  /// Whether the panel is folded down to its header.
  [[nodiscard]] bool panelCollapsed() const { return panel_collapsed_; }

  /// Fold the panel down to its header, giving the viewport its height.
  void collapsePanel();

  /// Unfold the panel.
  void expandPanel();

  /// Rows the folder pane currently lists, root first.
  [[nodiscard]] const std::vector<EditorAssetFolderRow>& folderRows() const {
    return rows_;
  }

  /// Index into `folderRows()` under a point, or -1.
  [[nodiscard]] int hitTestFolderRow(float x, float y) const;

  /// Rect of the folder row at @p index, in layout pixels.
  [[nodiscard]] Rect folderRowRect(size_t index) const;

  /// Folder whose assets the grid is showing.
  [[nodiscard]] size_t selectedFolder() const { return selected_folder_; }

  /// Show a folder's assets. Ignored when no such folder exists.
  void selectFolder(size_t folder);

  /// Whether a folder's children are listed.
  [[nodiscard]] bool folderExpanded(size_t folder) const {
    return expanded_.contains(folder);
  }

  /// List a folder's children beneath it.
  void expandFolder(size_t folder);

  /// Stop listing a folder's children.
  void collapseFolder(size_t folder);

  /// Assets the grid is showing, as indices into the whole asset list.
  [[nodiscard]] const std::vector<size_t>& visibleAssets() const;

  /// Grid slot under a point, or -1.
  [[nodiscard]] int hitTestCard(float x, float y) const;

  /// Rect of the card in @p slot, in layout pixels.
  [[nodiscard]] Rect cardRect(size_t slot) const;

  /// Number of assets listed across every folder.
  [[nodiscard]] size_t assetCount() const { return names_.size(); }

  /// Give an asset the texture its card draws. Indexed as the whole asset
  /// list is, so the editor names an asset the same way everywhere.
  void setAssetThumbnail(size_t asset, RhiTextureHandle texture);

  /// First grid slot whose card is on screen.
  [[nodiscard]] size_t firstVisibleSlot() const;

  /// How many slots from there are on screen. Together with the above this
  /// is what the editor generates pictures for, so an asset nobody has
  /// scrolled to is never opened.
  [[nodiscard]] size_t visibleSlotCount() const;

  /// Asset being dragged, as an index into the whole asset list, or -1 when
  /// no drag is in flight.
  [[nodiscard]] int draggingIndex() const { return dragging_; }

  /// Raised on release outside the panel, with an index into the whole
  /// asset list and the cursor position in layout pixels.
  std::function<void(size_t, float, float)> on_asset_dropped{};

private:
  /// Draw the panel heading and the selected folder's path.
  void renderHeader(const GuiDrawContext& ctx) const;
  /// Draw the folder pane's background, clipped to it.
  void renderNav(const GuiDrawContext& ctx) const;
  /// Draw every folder row that shows in the pane.
  void renderFolderRows(const GuiDrawContext& ctx) const;
  /// Draw one folder row's highlight, chevron, and name.
  void renderFolderRow(const GuiDrawContext& ctx, size_t index) const;
  /// Draw the cards clipped to the grid, or the empty-state line.
  void renderGrid(const GuiDrawContext& ctx) const;
  /// Draw the line shown in place of cards when a folder holds none.
  void renderEmptyGrid(const GuiDrawContext& ctx) const;
  /// Draw every card that shows in the grid.
  void renderCards(const GuiDrawContext& ctx) const;
  /// Draw one card's frame, label, and picture.
  void renderCard(const GuiDrawContext& ctx, size_t slot) const;
  /// Draw a card's picture, or the placeholder standing in for one.
  void renderCardPicture(const GuiDrawContext& ctx, const Rect& card,
                         size_t asset) const;
  /// Draw the ghost that follows the cursor mid-drag.
  void renderDragGhost(const GuiDrawContext& ctx) const;
  /// Flip one row's folder open or shut.
  void toggleFolder(const EditorAssetFolderRow& entry);
  /// Draw the header's two fold controls.
  void renderToggles(const GuiDrawContext& ctx) const;
  /// Act on a press in the header. Returns true when a control took it.
  bool pressHeader(const GuiMouseEvent& event);
  /// Fold the panel, or unfold it.
  void togglePanelFold();
  /// Fold the folder pane, or bring it back.
  void toggleNavFold();
  /// Handle a press in the folder pane. Returns false: the pane never
  /// captures, because opening a folder is done by the press alone.
  bool pressFolderPane(const GuiMouseEvent& event);
  /// Handle a press in the grid. Returns true when a drag began.
  bool pressGrid(const GuiMouseEvent& event);
  /// Rebuild the pane's rows after the tree or an expansion changed.
  void rebuildRows();
  /// Rebuild the header text after the selection or the tree changed.
  void rebuildHeaderText();
  /// Name to draw for a folder; the root is named for the directory itself.
  [[nodiscard]] std::string_view folderLabel(size_t folder) const;
  /// Pull both scroll offsets back inside what there is to scroll.
  void clampScroll();

  /// Folders and the asset indices they hold.
  EditorAssetTree tree_{};
  /// Display names, indexed as the tree's asset indices are.
  std::vector<std::string> names_{};
  /// Card pictures, indexed alongside `names_`. Not owned: the editor makes
  /// and destroys them, and hands the browser the handles to draw.
  std::vector<RhiTextureHandle> thumbnails_{};
  /// Folders whose children are listed.
  std::unordered_set<size_t> expanded_{};
  /// Flattened pane rows, rebuilt whenever the tree or expansion changes.
  std::vector<EditorAssetFolderRow> rows_{};
  /// Folder the grid is showing.
  size_t selected_folder_ = EDITOR_ASSET_FOLDER_ROOT;
  /// Backing store for the header's string_view.
  std::string header_text_{};
  /// Asset being dragged, or -1.
  int dragging_ = -1;
  /// Last cursor X seen during a drag.
  float drag_x_ = 0.0f;
  /// Last cursor Y seen during a drag.
  float drag_y_ = 0.0f;
  /// How far the folder pane is scrolled, in pixels.
  float nav_scroll_ = 0.0f;
  /// How far the grid is scrolled, in pixels.
  float grid_scroll_ = 0.0f;
  /// Whether the folder pane is folded away.
  bool nav_collapsed_ = false;
  /// Whether the panel is folded down to its header.
  bool panel_collapsed_ = false;
};

}  // namespace eng::editor
