#include <algorithm>
#include <cmath>
#include <editor/shell/editor-asset-browser-widget.h>
#include <engine/gui/gui-color.h>
#include <engine/gui/gui-draw-context.h>
#include <engine/gui/gui-theme-constants.h>
#include <utility>

namespace eng::editor {

namespace {

  constexpr float SIDE_PADDING = 10.0f;
  constexpr float LABEL_INSET = 8.0f;
  constexpr float TEXT_DROP = 7.0f;
  constexpr float ROW_TEXT_DROP = 4.0f;
  /// Pixels one wheel notch moves a pane.
  constexpr float SCROLL_STEP = 24.0f;

  constexpr GuiColor CARD_FILL{52, 52, 58, 255};
  constexpr GuiColor CARD_BORDER{72, 72, 80, 255};
  constexpr GuiColor GHOST_FILL{0, 122, 204, 150};
  /// The folder pane reads as recessed, so it takes the editor's backdrop
  /// token rather than the panel's own fill.
  constexpr GuiColor NAV_FILL = THEME_BG;
  /// The selected row, as the accent laid over whatever is beneath it.
  constexpr GuiColor ROW_SELECTED{THEME_ACCENT.r, THEME_ACCENT.g,
                                  THEME_ACCENT.b, 90};

  /// Sunk area a card's picture drops into, drawn whether or not one has
  /// arrived, so a card does not change shape when it does. The backdrop
  /// token again, as the folder pane uses: recessed surfaces read alike.
  constexpr GuiColor PICTURE_WELL = THEME_BG;
  /// Untinted: a thumbnail is a picture, not an icon to be coloured.
  constexpr GuiColor PICTURE_TINT{255, 255, 255, 255};

  /// Name shown for the root, which has none of its own.
  constexpr std::string_view ROOT_LABEL = "assets";

}  // namespace

EditorAssetBrowserWidget::EditorAssetBrowserWidget() {
  widget_type = GuiWidgetType::PANEL;
  debug_name = "editor-asset-browser";
  fill_color = THEME_PANEL;
  border_color = THEME_BORDER;
  border_width = 1.0f;
  expanded_.insert(EDITOR_ASSET_FOLDER_ROOT);
  rebuildRows();
  rebuildHeaderText();
}

std::unique_ptr<GuiWidget> EditorAssetBrowserWidget::clone() const {
  return std::make_unique<EditorAssetBrowserWidget>(*this);
}

EditorAssetBrowserLayout EditorAssetBrowserWidget::layout() const {
  return layoutAssetBrowser({rect, nav_collapsed_, panel_collapsed_});
}

float EditorAssetBrowserWidget::preferredHeight() const {
  return panel_collapsed_ ? ASSET_PANEL_COLLAPSED_HEIGHT : ASSET_PANEL_HEIGHT;
}

void EditorAssetBrowserWidget::hideFolderPane() {
  nav_collapsed_ = true;
  clampScroll();
}

void EditorAssetBrowserWidget::showFolderPane() {
  nav_collapsed_ = false;
  clampScroll();
}

void EditorAssetBrowserWidget::collapsePanel() {
  panel_collapsed_ = true;
  // A drag cannot survive the grid it started in going away.
  dragging_ = -1;
}

void EditorAssetBrowserWidget::expandPanel() {
  panel_collapsed_ = false;
  clampScroll();
}

const std::vector<size_t>& EditorAssetBrowserWidget::visibleAssets() const {
  return tree_.folders[selected_folder_].assets;
}

std::string_view EditorAssetBrowserWidget::folderLabel(size_t folder) const {
  const EditorAssetFolder& node = tree_.folders[folder];
  return node.name.empty() ? ROOT_LABEL : std::string_view(node.name);
}

void EditorAssetBrowserWidget::setAssets(EditorAssetTree tree,
                                         std::vector<std::string> names) {
  tree_ = std::move(tree);
  // Every lookup here indexes `folders` directly, and a tree without a root
  // would make all of them unsafe.
  if (tree_.folders.empty()) {
    tree_.folders.emplace_back();
  }
  names_ = std::move(names);
  thumbnails_.assign(names_.size(), RHI_TEXTURE_INVALID);
  selected_folder_ = EDITOR_ASSET_FOLDER_ROOT;
  dragging_ = -1;
  nav_scroll_ = 0.0f;
  grid_scroll_ = 0.0f;
  // Cleared, not carried over: what is expanded is remembered as folder
  // indices, and the same index in a new tree is a different folder. Keeping
  // the set across a rescan would open folders nobody opened, and opening
  // another project would open them from the shape of the one before it.
  expanded_.clear();
  expanded_.insert(EDITOR_ASSET_FOLDER_ROOT);
  rebuildRows();
  rebuildHeaderText();
}

void EditorAssetBrowserWidget::setAssetThumbnail(size_t asset,
                                                 RhiTextureHandle texture) {
  if (asset >= thumbnails_.size()) {
    return;
  }
  thumbnails_[asset] = texture;
}

size_t EditorAssetBrowserWidget::firstVisibleSlot() const {
  return assetFirstVisibleSlot(layout().grid, grid_scroll_);
}

size_t EditorAssetBrowserWidget::visibleSlotCount() const {
  return assetVisibleSlotCount(layout().grid, visibleAssets().size(),
                               grid_scroll_);
}

void EditorAssetBrowserWidget::rebuildRows() {
  rows_ = flattenAssetFolderRows(tree_, expanded_);
  clampScroll();
}

void EditorAssetBrowserWidget::rebuildHeaderText() {
  const EditorAssetFolder& folder = tree_.folders[selected_folder_];
  // A folder standing for no directory is a section of its own — the
  // built-in General one — and the header names it rather than filing it
  // under a directory it does not come from.
  if (folder.relative_path.empty() && !folder.name.empty()) {
    header_text_ = folder.name;
    return;
  }
  // The pane already names the root; the header names the panel, and then
  // says how far into it the grid is showing.
  header_text_ = "Assets";
  if (!folder.relative_path.empty()) {
    header_text_ += " — " + folder.relative_path.generic_string();
  }
}

void EditorAssetBrowserWidget::clampScroll() {
  const EditorAssetBrowserLayout regions = layout();
  nav_scroll_ = clampAssetScroll(
      nav_scroll_, assetNavContentHeight(rows_.size()), regions.nav.h);
  grid_scroll_ = clampAssetScroll(
      grid_scroll_,
      assetGridContentHeight(regions.grid, visibleAssets().size()),
      regions.grid.h);
}

void EditorAssetBrowserWidget::selectFolder(size_t folder) {
  if (folder >= tree_.folders.size()) {
    return;
  }
  selected_folder_ = folder;
  grid_scroll_ = 0.0f;
  rebuildHeaderText();
}

void EditorAssetBrowserWidget::expandFolder(size_t folder) {
  expanded_.insert(folder);
  rebuildRows();
}

void EditorAssetBrowserWidget::collapseFolder(size_t folder) {
  expanded_.erase(folder);
  rebuildRows();
}

Rect EditorAssetBrowserWidget::folderRowRect(size_t index) const {
  return assetFolderRowRect(layout().nav, index, nav_scroll_);
}

int EditorAssetBrowserWidget::hitTestFolderRow(float x, float y) const {
  const Rect nav = layout().nav;
  if (!visible || rows_.empty() || !containsPoint(nav, x, y)) {
    return -1;
  }
  const float offset = (y - nav.y) + nav_scroll_;
  const auto index = static_cast<size_t>(std::floor(offset / ASSET_ROW_HEIGHT));
  return index < rows_.size() ? static_cast<int>(index) : -1;
}

Rect EditorAssetBrowserWidget::cardRect(size_t slot) const {
  return assetCardRect(layout().grid, slot, grid_scroll_);
}

int EditorAssetBrowserWidget::hitTestCard(float x, float y) const {
  const Rect grid = layout().grid;
  if (!visible || !containsPoint(grid, x, y)) {
    return -1;
  }
  // Walked rather than solved, so the gaps between cards miss rather than
  // rounding onto a neighbour.
  for (size_t slot = 0; slot < visibleAssets().size(); ++slot) {
    if (containsPoint(cardRect(slot), x, y)) {
      return static_cast<int>(slot);
    }
  }
  return -1;
}

void EditorAssetBrowserWidget::toggleFolder(const EditorAssetFolderRow& entry) {
  if (entry.expanded) {
    collapseFolder(entry.folder);
  } else {
    expandFolder(entry.folder);
  }
}

bool EditorAssetBrowserWidget::pressFolderPane(const GuiMouseEvent& event) {
  const int row = hitTestFolderRow(event.x, event.y);
  if (row < 0) {
    return false;
  }
  // A copy, not a reference: opening a folder rebuilds the rows this would
  // otherwise point into.
  const EditorAssetFolderRow entry = rows_[static_cast<size_t>(row)];
  const Rect chevron = assetFolderChevronRect(
      folderRowRect(static_cast<size_t>(row)), entry.depth);
  if (entry.has_children && containsPoint(chevron, event.x, event.y)) {
    toggleFolder(entry);
    return false;
  }
  selectFolder(entry.folder);
  return false;
}

bool EditorAssetBrowserWidget::pressGrid(const GuiMouseEvent& event) {
  const int slot = hitTestCard(event.x, event.y);
  if (slot < 0) {
    return false;
  }
  dragging_ = static_cast<int>(visibleAssets()[static_cast<size_t>(slot)]);
  drag_x_ = event.x;
  drag_y_ = event.y;
  return true;
}

void EditorAssetBrowserWidget::togglePanelFold() {
  if (panel_collapsed_) {
    expandPanel();
  } else {
    collapsePanel();
  }
}

void EditorAssetBrowserWidget::toggleNavFold() {
  if (nav_collapsed_) {
    showFolderPane();
  } else {
    hideFolderPane();
  }
}

bool EditorAssetBrowserWidget::pressHeader(const GuiMouseEvent& event) {
  const Rect header = layout().header;
  if (containsPoint(assetPanelToggleRect(header), event.x, event.y)) {
    togglePanelFold();
    return true;
  }
  // The pane's control is hidden while the panel is folded, so a press
  // where it would be must not act on it.
  if (panel_collapsed_ ||
      !containsPoint(assetNavToggleRect(header), event.x, event.y)) {
    return false;
  }
  toggleNavFold();
  return true;
}

bool EditorAssetBrowserWidget::handleMouseDown(const GuiMouseEvent& event) {
  if (event.button != GuiMouseButton::LEFT || !visible) {
    return false;
  }
  // A fold control takes the press, but there is nothing to capture: the
  // panel changed shape and no drag began.
  if (pressHeader(event)) {
    return false;
  }
  if (containsPoint(layout().nav, event.x, event.y)) {
    return pressFolderPane(event);
  }
  return pressGrid(event);
}

void EditorAssetBrowserWidget::handleMouseMove(const GuiMouseEvent& event) {
  if (dragging_ < 0) {
    return;
  }
  drag_x_ = event.x;
  drag_y_ = event.y;
}

void EditorAssetBrowserWidget::handleMouseUp(const GuiMouseEvent& event) {
  if (dragging_ < 0) {
    return;
  }
  const int dropped = dragging_;
  dragging_ = -1;
  // A release back inside the panel is a cancelled drag, not a placement.
  if (containsPoint(rect, event.x, event.y) || !on_asset_dropped) {
    return;
  }
  on_asset_dropped(static_cast<size_t>(dropped), event.x, event.y);
}

bool EditorAssetBrowserWidget::handleScroll(const GuiScrollEvent& event) {
  if (!visible) {
    return false;
  }
  const EditorAssetBrowserLayout regions = layout();
  // Positive delta_y is away from the user, which walks content upward.
  const float step = event.delta_y * SCROLL_STEP;
  if (containsPoint(regions.nav, event.x, event.y)) {
    nav_scroll_ -= step;
  } else if (containsPoint(regions.grid, event.x, event.y)) {
    grid_scroll_ -= step;
  } else {
    return false;
  }
  clampScroll();
  return true;
}

void EditorAssetBrowserWidget::renderToggles(const GuiDrawContext& ctx) const {
  const Rect header = layout().header;
  const auto color = GuiColor::applyOpacity(THEME_DIM, opacity);
  ctx.drawText(color,
               drawPosInset(assetPanelToggleRect(header), 6.0f, TEXT_DROP),
               panel_collapsed_ ? "^" : "v");
  if (panel_collapsed_) {
    return;
  }
  ctx.drawText(color, drawPosInset(assetNavToggleRect(header), 6.0f, TEXT_DROP),
               nav_collapsed_ ? ">" : "<");
}

void EditorAssetBrowserWidget::renderHeader(const GuiDrawContext& ctx) const {
  const Rect header = layout().header;
  // The title clears the fold control on the left rather than starting at
  // the panel edge, so the two never sit on top of each other.
  const DrawPos title{header.x + ASSET_TOGGLE_WIDTH + SIDE_PADDING,
                      header.y + TEXT_DROP};
  ctx.drawText(GuiColor::applyOpacity(THEME_DIM, opacity), title, header_text_);
  renderToggles(ctx);
}

void EditorAssetBrowserWidget::renderFolderRow(const GuiDrawContext& ctx,
                                               size_t index) const {
  const EditorAssetFolderRow& entry = rows_[index];
  const Rect row = folderRowRect(index);
  if (entry.folder == selected_folder_) {
    ctx.drawFilledRect(row, GuiColor::applyOpacity(ROW_SELECTED, opacity));
  }
  if (entry.has_children) {
    const Rect chevron = assetFolderChevronRect(row, entry.depth);
    ctx.drawText(GuiColor::applyOpacity(THEME_DIM, opacity),
                 drawPosInset(chevron, 0.0f, ROW_TEXT_DROP),
                 entry.expanded ? "v" : ">");
  }
  const DrawPos label{assetFolderLabelX(row, entry.depth),
                      row.y + ROW_TEXT_DROP};
  ctx.drawText(GuiColor::applyOpacity(THEME_TEXT, opacity), label,
               folderLabel(entry.folder));
}

void EditorAssetBrowserWidget::renderFolderRows(
    const GuiDrawContext& ctx) const {
  const Rect nav = layout().nav;
  for (size_t i = 0; i < rows_.size(); ++i) {
    // Rows entirely outside the pane are skipped rather than clipped, which
    // is the same picture for less work.
    const Rect row = folderRowRect(i);
    if (row.y + row.h > nav.y && row.y < nav.y + nav.h) {
      renderFolderRow(ctx, i);
    }
  }
}

void EditorAssetBrowserWidget::renderNav(const GuiDrawContext& ctx) const {
  const Rect nav = layout().nav;
  if (nav.w <= 0.0f || ctx.renderer == nullptr) {
    return;
  }
  ctx.drawFilledRect(nav, GuiColor::applyOpacity(NAV_FILL, opacity));
  // Scrolling walks rows past the top of the pane, and the row half way out
  // has to be cut off there rather than drawn across the header and the
  // viewport above it.
  ctx.renderer->pushScissor(nav);
  renderFolderRows(ctx);
  ctx.renderer->popScissor();
}

void EditorAssetBrowserWidget::renderCardPicture(const GuiDrawContext& ctx,
                                                 const Rect& card,
                                                 size_t asset) const {
  const Rect picture = assetCardThumbnailRect(card);
  if (picture.w <= 0.0f || picture.h <= 0.0f) {
    return;
  }
  const RhiTextureHandle texture =
      asset < thumbnails_.size() ? thumbnails_[asset] : RHI_TEXTURE_INVALID;
  if (texture == RHI_TEXTURE_INVALID) {
    // A well for the picture to drop into, so a card does not change shape
    // the moment one arrives.
    ctx.drawFilledRect(picture, GuiColor::applyOpacity(PICTURE_WELL, opacity));
    return;
  }
  ctx.drawTexturedRect(
      {picture, texture, GuiColor::applyOpacity(PICTURE_TINT, opacity)});
}

void EditorAssetBrowserWidget::renderCard(const GuiDrawContext& ctx,
                                          size_t slot) const {
  const Rect card = cardRect(slot);
  const size_t asset = visibleAssets()[slot];
  ctx.drawFilledRect(card, GuiColor::applyOpacity(CARD_FILL, opacity));
  ctx.drawBorderRect(card, GuiColor::applyOpacity(CARD_BORDER, opacity));
  ctx.drawText(GuiColor::applyOpacity(THEME_TEXT, opacity),
               drawPosInset(card, LABEL_INSET, LABEL_INSET), names_[asset]);
  renderCardPicture(ctx, card, asset);
}

void EditorAssetBrowserWidget::renderEmptyGrid(
    const GuiDrawContext& ctx) const {
  ctx.drawText(GuiColor::applyOpacity(THEME_DIM, opacity),
               drawPosInset(layout().grid, SIDE_PADDING, TEXT_DROP),
               "Drop .obj files into the project's assets/ folder");
}

void EditorAssetBrowserWidget::renderCards(const GuiDrawContext& ctx) const {
  const Rect grid = layout().grid;
  for (size_t slot = 0; slot < visibleAssets().size(); ++slot) {
    // As the folder pane: cards wholly outside the grid are skipped rather
    // than clipped.
    const Rect card = cardRect(slot);
    if (card.y + card.h > grid.y && card.y < grid.y + grid.h) {
      renderCard(ctx, slot);
    }
  }
}

void EditorAssetBrowserWidget::renderGrid(const GuiDrawContext& ctx) const {
  if (visibleAssets().empty()) {
    renderEmptyGrid(ctx);
    return;
  }
  if (ctx.renderer == nullptr) {
    return;
  }
  // A card scrolled half way out of the grid is cut off at its edge, not
  // drawn across the chrome above the panel.
  ctx.renderer->pushScissor(layout().grid);
  renderCards(ctx);
  ctx.renderer->popScissor();
}

void EditorAssetBrowserWidget::renderDragGhost(
    const GuiDrawContext& ctx) const {
  if (dragging_ < 0) {
    return;
  }
  const Rect ghost{drag_x_ - ASSET_CARD_WIDTH * 0.5f,
                   drag_y_ - ASSET_CARD_HEIGHT * 0.5f, ASSET_CARD_WIDTH,
                   ASSET_CARD_HEIGHT};
  ctx.drawFilledRect(ghost, GuiColor::applyOpacity(GHOST_FILL, opacity));
  ctx.drawText(GuiColor::applyOpacity(THEME_TEXT, opacity),
               drawPosInset(ghost, LABEL_INSET, LABEL_INSET),
               names_[static_cast<size_t>(dragging_)]);
}

void EditorAssetBrowserWidget::render(const GuiDrawContext& ctx) const {
  if (rect.w <= 0.0f || rect.h <= 0.0f) {
    return;
  }
  renderPanel({ctx, GuiColor::applyOpacity(fill_color, opacity)});
  ctx.drawBorderRect(rect, GuiColor::applyOpacity(THEME_BORDER, opacity));
  renderHeader(ctx);
  if (!panel_collapsed_) {
    renderNav(ctx);
    renderGrid(ctx);
  }
  // Last, and outside either pane's bounds check: the ghost follows the
  // cursor anywhere on screen, including over the panes it started in.
  renderDragGhost(ctx);
}

}  // namespace eng::editor
