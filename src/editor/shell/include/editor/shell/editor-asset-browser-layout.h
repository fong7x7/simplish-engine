#pragma once

/// @file editor-asset-browser-layout.h
/// @brief Where the asset browser's regions, folder rows, and cards sit.
/// @par Threading Thread-safe (pure functions over value types).

#include <cstddef>
#include <editor/shell/editor-asset-browser-layout-params.h>
#include <engine/gui/gui-rect.h>

namespace eng::editor {

/// Height of the browser panel in logical pixels.
///
/// Taller than a single row of cards needs, because the folder pane beside
/// them has to show enough of a hierarchy to be worth navigating.
inline constexpr float ASSET_PANEL_HEIGHT = 220.0f;
/// Width of the folder pane down the left.
inline constexpr float ASSET_NAV_WIDTH = 190.0f;
/// Height of the panel's title strip.
inline constexpr float ASSET_HEADER_HEIGHT = 22.0f;
/// Height of one folder row.
inline constexpr float ASSET_ROW_HEIGHT = 20.0f;
/// Horizontal indent per level of nesting.
inline constexpr float ASSET_ROW_INDENT = 12.0f;
/// Width of a row's open/close control, measured from the row's indent.
inline constexpr float ASSET_CHEVRON_WIDTH = 12.0f;
/// Card width.
inline constexpr float ASSET_CARD_WIDTH = 104.0f;
/// Card height.
inline constexpr float ASSET_CARD_HEIGHT = 88.0f;
/// Gap between cards, and between a card and the edge it sits against.
inline constexpr float ASSET_CARD_GAP = 8.0f;
/// Height of the panel folded down to its header alone.
inline constexpr float ASSET_PANEL_COLLAPSED_HEIGHT = ASSET_HEADER_HEIGHT;
/// Width of one of the header's fold controls.
inline constexpr float ASSET_TOGGLE_WIDTH = 20.0f;

/// The regions a browser panel divides into.
/// @thread_safety Immutable value type.
struct EditorAssetBrowserLayout {
  /// Title strip across the top.
  Rect header{};
  /// Folder pane down the left.
  Rect nav{};
  /// Card area filling what is left.
  Rect grid{};
};

/// Divide a panel into its header, folder pane, and card area.
///
/// A panel too narrow for the folder pane gives it whatever is there and
/// leaves the cards nothing, rather than handing either a negative width. A
/// folded panel is its header and nothing else: both other regions come
/// back zero-sized, so neither draws nor answers a hit test.
[[nodiscard]] EditorAssetBrowserLayout
layoutAssetBrowser(const EditorAssetBrowserLayoutParams& params);

/// The control that folds the whole panel, at the header's right edge.
[[nodiscard]] Rect assetPanelToggleRect(const Rect& header);

/// The control that folds the folder pane, at the header's left edge.
[[nodiscard]] Rect assetNavToggleRect(const Rect& header);

/// Rect of the folder row at @p index within @p nav, scrolled by @p scroll_y.
/// Rows outside the pane still get a rect; the caller clips them.
[[nodiscard]] Rect assetFolderRowRect(const Rect& nav, size_t index,
                                      float scroll_y);

/// The open/close control within a folder row at @p depth.
[[nodiscard]] Rect assetFolderChevronRect(const Rect& row, uint32_t depth);

/// Where a folder row's label starts, given its depth.
[[nodiscard]] float assetFolderLabelX(const Rect& row, uint32_t depth);

/// How many cards fit across @p grid. Never zero: a pane too narrow for a
/// card still lays them out in a single column rather than dividing by it.
[[nodiscard]] size_t assetCardsPerRow(const Rect& grid);

/// Rect of the card in @p slot within @p grid, scrolled by @p scroll_y.
[[nodiscard]] Rect assetCardRect(const Rect& grid, size_t slot, float scroll_y);

/// Height @p count cards need in @p grid, for clamping the scroll.
[[nodiscard]] float assetGridContentHeight(const Rect& grid, size_t count);

/// Height @p row_count folder rows need, for clamping the scroll.
[[nodiscard]] float assetNavContentHeight(size_t row_count);

/// Clamp @p offset to what is actually scrollable: zero when the content
/// fits, and never past its end when it does not.
[[nodiscard]] float clampAssetScroll(float offset, float content_height,
                                     float view_height);

}  // namespace eng::editor
