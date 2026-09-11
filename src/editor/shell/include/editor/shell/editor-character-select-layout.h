#pragma once

/// @file editor-character-select-layout.h
/// @brief Where the character selector's panel and cards sit.
/// @par Threading Thread-safe (pure functions over value types).

#include <cstddef>
#include <engine/gui/gui-rect.h>

namespace eng::editor {

/// Width of one character card.
inline constexpr float CHARACTER_CARD_WIDTH = 136.0f;

/// Height of one character card: a square picture, then three lines.
inline constexpr float CHARACTER_CARD_HEIGHT = 196.0f;

/// Gap between two cards, and around the panel's contents.
inline constexpr float CHARACTER_CARD_GAP = 12.0f;

/// Height of the selector's title line, and of its hint line.
inline constexpr float CHARACTER_SELECT_LINE_HEIGHT = 28.0f;

/// Where the selector's parts sit for one area and card count.
/// @thread_safety Immutable value type.
struct EditorCharacterSelectLayout {
  /// The panel the cards sit on, centred in the area.
  Rect panel{};
  /// The title line across its top.
  Rect title{};
  /// The hint line across its bottom.
  Rect hint{};
  /// Where the first card's top-left corner is.
  Rect cards{};
  /// Cards to a row: as many as the area is wide enough for.
  size_t columns = 1;
};

/// Lay @p count cards out as a grid on a panel centred in @p area: as many
/// to a row as fit, and as many rows as that takes. A panel larger than the
/// area overflows it evenly rather than moving off to one side.
[[nodiscard]] EditorCharacterSelectLayout
layoutEditorCharacterSelect(const Rect& area, size_t count);

/// The rect of card @p index.
[[nodiscard]] Rect characterCardRect(const EditorCharacterSelectLayout& layout,
                                     size_t index);

/// The square a card's picture fills, across its top.
[[nodiscard]] Rect characterCardPictureRect(const Rect& card);

/// Index of the card under a point among the first @p count, or -1. The
/// gaps between cards belong to none of them.
[[nodiscard]] int
hitTestCharacterCard(const EditorCharacterSelectLayout& layout, size_t count,
                     float x, float y);

}  // namespace eng::editor
