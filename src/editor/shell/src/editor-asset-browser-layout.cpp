#include <algorithm>
#include <editor/shell/editor-asset-browser-layout.h>

namespace eng::editor {

EditorAssetBrowserLayout layoutAssetBrowser(const Rect& panel) {
  EditorAssetBrowserLayout out;
  out.header = makeRect(panel.x, panel.y, panel.w, ASSET_HEADER_HEIGHT);
  const float body_y = panel.y + ASSET_HEADER_HEIGHT;
  const float body_h = std::max(0.0f, panel.h - ASSET_HEADER_HEIGHT);
  const float nav_w = std::min(ASSET_NAV_WIDTH, std::max(0.0f, panel.w));
  out.nav = makeRect(panel.x, body_y, nav_w, body_h);
  out.grid = makeRect(panel.x + nav_w, body_y, std::max(0.0f, panel.w - nav_w),
                      body_h);
  return out;
}

Rect assetFolderRowRect(const Rect& nav, size_t index, float scroll_y) {
  const float y =
      nav.y + (static_cast<float>(index) * ASSET_ROW_HEIGHT) - scroll_y;
  return makeRect(nav.x, y, nav.w, ASSET_ROW_HEIGHT);
}

Rect assetFolderChevronRect(const Rect& row, uint32_t depth) {
  const float x = row.x + (static_cast<float>(depth) * ASSET_ROW_INDENT);
  return makeRect(x, row.y, ASSET_CHEVRON_WIDTH, row.h);
}

float assetFolderLabelX(const Rect& row, uint32_t depth) {
  return assetFolderChevronRect(row, depth).x + ASSET_CHEVRON_WIDTH;
}

size_t assetCardsPerRow(const Rect& grid) {
  const float usable = grid.w - ASSET_CARD_GAP;
  const float per_card = ASSET_CARD_WIDTH + ASSET_CARD_GAP;
  if (usable < per_card) {
    return 1;
  }
  return static_cast<size_t>(usable / per_card);
}

Rect assetCardRect(const Rect& grid, size_t slot, float scroll_y) {
  const size_t per_row = assetCardsPerRow(grid);
  const float column = static_cast<float>(slot % per_row);
  const float row = static_cast<float>(slot / per_row);
  const float x =
      grid.x + ASSET_CARD_GAP + (column * (ASSET_CARD_WIDTH + ASSET_CARD_GAP));
  const float y = grid.y + ASSET_CARD_GAP +
                  (row * (ASSET_CARD_HEIGHT + ASSET_CARD_GAP)) - scroll_y;
  return makeRect(x, y, ASSET_CARD_WIDTH, ASSET_CARD_HEIGHT);
}

float assetGridContentHeight(const Rect& grid, size_t count) {
  if (count == 0) {
    return 0.0f;
  }
  const size_t per_row = assetCardsPerRow(grid);
  const size_t rows = ((count - 1) / per_row) + 1;
  return ASSET_CARD_GAP +
         (static_cast<float>(rows) * (ASSET_CARD_HEIGHT + ASSET_CARD_GAP));
}

float assetNavContentHeight(size_t row_count) {
  return static_cast<float>(row_count) * ASSET_ROW_HEIGHT;
}

float clampAssetScroll(float offset, float content_height, float view_height) {
  const float overflow = content_height - view_height;
  if (overflow <= 0.0f) {
    return 0.0f;
  }
  return std::clamp(offset, 0.0f, overflow);
}

}  // namespace eng::editor
