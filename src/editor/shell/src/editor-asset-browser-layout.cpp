#include <algorithm>
#include <cmath>
#include <editor/shell/editor-asset-browser-layout.h>

namespace eng::editor {

namespace {

  /// Split the area below the header between the folder pane and the cards.
  void layoutBody(const EditorAssetBrowserLayoutParams& params,
                  const Rect& body, EditorAssetBrowserLayout& out) {
    const float nav_w = params.nav_collapsed
                            ? 0.0f
                            : std::min(ASSET_NAV_WIDTH, std::max(0.0f, body.w));
    out.nav = makeRect(body.x, body.y, nav_w, body.h);
    out.grid = makeRect(body.x + nav_w, body.y, std::max(0.0f, body.w - nav_w),
                        body.h);
  }

}  // namespace

EditorAssetBrowserLayout
layoutAssetBrowser(const EditorAssetBrowserLayoutParams& params) {
  const Rect& panel = params.panel;
  EditorAssetBrowserLayout out;
  out.header = makeRect(panel.x, panel.y, panel.w, ASSET_HEADER_HEIGHT);
  if (params.panel_collapsed) {
    // Both other regions stay zero-sized, which is what keeps a folded
    // panel from drawing or answering a hit test below its header.
    return out;
  }
  const Rect body = makeRect(panel.x, panel.y + ASSET_HEADER_HEIGHT, panel.w,
                             std::max(0.0f, panel.h - ASSET_HEADER_HEIGHT));
  layoutBody(params, body, out);
  return out;
}

Rect assetPanelToggleRect(const Rect& header) {
  return makeRect(header.x + header.w - ASSET_TOGGLE_WIDTH, header.y,
                  ASSET_TOGGLE_WIDTH, header.h);
}

Rect assetNavToggleRect(const Rect& header) {
  return makeRect(header.x, header.y, ASSET_TOGGLE_WIDTH, header.h);
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


Rect assetCardThumbnailRect(const Rect& card) {
  const float across =
      std::max(0.0f, card.w - (2.0f * ASSET_CARD_PICTURE_INSET));
  const float down = std::max(0.0f, card.h - ASSET_CARD_LABEL_HEIGHT -
                                        ASSET_CARD_PICTURE_INSET);
  // The largest square that fits. A rect any other shape would stretch the
  // picture, which is rendered square and drawn across the whole of it.
  const float side = std::min(across, down);
  return makeRect(card.x + ((card.w - side) * 0.5f),
                  card.y + ASSET_CARD_LABEL_HEIGHT, side, side);
}

size_t assetFirstVisibleSlot(const Rect& grid, float scroll_y) {
  const float pitch = ASSET_CARD_HEIGHT + ASSET_CARD_GAP;
  const float above = std::max(0.0f, scroll_y - ASSET_CARD_GAP);
  const auto row = static_cast<size_t>(std::floor(above / pitch));
  return row * assetCardsPerRow(grid);
}

size_t assetVisibleSlotCount(const Rect& grid, size_t count, float scroll_y) {
  if (count == 0 || grid.h <= 0.0f) {
    return 0;
  }
  const size_t first = assetFirstVisibleSlot(grid, scroll_y);
  if (first >= count) {
    return 0;
  }
  const float pitch = ASSET_CARD_HEIGHT + ASSET_CARD_GAP;
  const float reach = grid.h + scroll_y - ASSET_CARD_GAP;
  const auto last_row = static_cast<size_t>(std::floor(reach / pitch));
  const size_t per_row = assetCardsPerRow(grid);
  const size_t past_end = std::min(count, (last_row + 1) * per_row);
  return past_end > first ? past_end - first : 0;
}

}  // namespace eng::editor
