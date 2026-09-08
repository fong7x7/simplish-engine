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
/// Exactly one row of cards, gaps and header included. A second row would
/// take another 132px off the viewport for assets nobody has scrolled to;
/// the grid scrolls to reach them instead.
inline constexpr float ASSET_PANEL_HEIGHT = 162.0f;
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
/// Card height: its label, a square picture, and the margin under it.
///
/// Derived rather than chosen, so the picture stays square. A card shorter
/// than this leaves the picture less height than width, and the picture is
/// what the card is for.
inline constexpr float ASSET_CARD_HEIGHT = 124.0f;
/// Gap between cards, and between a card and the edge it sits against.
inline constexpr float ASSET_CARD_GAP = 8.0f;
/// Room reserved at the top of a card for its name.
inline constexpr float ASSET_CARD_LABEL_HEIGHT = 24.0f;
/// Margin between a card's picture and its own edges.
inline constexpr float ASSET_CARD_PICTURE_INSET = 4.0f;
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

/// Where a card's picture goes, under the room its label takes.
///
/// Always square, and never wider than the card: thumbnails are rendered
/// square and drawn across the whole rect they are given, so a rect of any
/// other shape stretches the model rather than framing it.
[[nodiscard]] Rect assetCardThumbnailRect(const Rect& card);

/// First slot whose card reaches into @p grid at @p scroll_y.
///
/// This and the count below are what let the editor generate pictures only
/// for the cards someone is actually looking at. They err towards a slot
/// too many rather than too few: a picture made a moment early costs a
/// little work, one made late is a card that stayed blank.
[[nodiscard]] size_t assetFirstVisibleSlot(const Rect& grid, float scroll_y);

/// How many slots from `assetFirstVisibleSlot` reach into @p grid, out of
/// @p count cards in total.
[[nodiscard]] size_t assetVisibleSlotCount(const Rect& grid, size_t count,
                                           float scroll_y);

}  // namespace eng::editor
