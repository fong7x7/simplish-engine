#include <algorithm>
#include <editor/shell/editor-character-select-layout.h>

namespace eng::editor {

namespace {

  /// Cards to a row that fit in @p width, at least one and at most
  /// @p count.
  size_t columnsFor(float width, size_t count) {
    const float usable = width - CHARACTER_CARD_GAP;
    const auto fit = static_cast<size_t>(
        std::max(1.0f, usable / (CHARACTER_CARD_WIDTH + CHARACTER_CARD_GAP)));
    return std::clamp<size_t>(fit, 1, std::max<size_t>(count, 1));
  }

  /// How far @p n cards and the gaps around them reach, at @p size each.
  float span(size_t n, float size) {
    const auto cards = static_cast<float>(n);
    return cards * size + (cards + 1.0f) * CHARACTER_CARD_GAP;
  }

  /// The title, hint and first card, from where the panel is.
  void placeLines(EditorCharacterSelectLayout& layout) {
    const Rect& panel = layout.panel;
    layout.title =
        makeRect(panel.x, panel.y, panel.w, CHARACTER_SELECT_LINE_HEIGHT);
    layout.hint =
        makeRect(panel.x, panel.y + panel.h - CHARACTER_SELECT_LINE_HEIGHT,
                 panel.w, CHARACTER_SELECT_LINE_HEIGHT);
    layout.cards = makeRect(panel.x + CHARACTER_CARD_GAP,
                            layout.title.y + layout.title.h, 0.0f, 0.0f);
  }

}  // namespace

EditorCharacterSelectLayout layoutEditorCharacterSelect(const Rect& area,
                                                        size_t count) {
  EditorCharacterSelectLayout layout;
  layout.columns = columnsFor(area.w, count);
  const size_t rows =
      (std::max<size_t>(count, 1) + layout.columns - 1) / layout.columns;
  const float width = span(layout.columns, CHARACTER_CARD_WIDTH);
  // The title and hint lines are the space above and below the cards.
  const float height =
      static_cast<float>(rows) * (CHARACTER_CARD_HEIGHT + CHARACTER_CARD_GAP) -
      CHARACTER_CARD_GAP + 2.0f * CHARACTER_SELECT_LINE_HEIGHT;
  layout.panel = makeRect(area.x + (area.w - width) * 0.5f,
                          area.y + (area.h - height) * 0.5f, width, height);
  placeLines(layout);
  return layout;
}

Rect characterCardRect(const EditorCharacterSelectLayout& layout,
                       size_t index) {
  const size_t column_index = index % layout.columns;
  const size_t row_index = index / layout.columns;
  const auto column = static_cast<float>(column_index);
  const auto row = static_cast<float>(row_index);
  return makeRect(
      layout.cards.x + column * (CHARACTER_CARD_WIDTH + CHARACTER_CARD_GAP),
      layout.cards.y + row * (CHARACTER_CARD_HEIGHT + CHARACTER_CARD_GAP),
      CHARACTER_CARD_WIDTH, CHARACTER_CARD_HEIGHT);
}

Rect characterCardPictureRect(const Rect& card) {
  const float inset = CHARACTER_CARD_GAP * 0.5f;
  const float side = card.w - 2.0f * inset;
  return makeRect(card.x + inset, card.y + inset, side, side);
}

int hitTestCharacterCard(const EditorCharacterSelectLayout& layout,
                         size_t count, float x, float y) {
  for (size_t i = 0; i < count; ++i) {
    if (containsPoint(characterCardRect(layout, i), x, y)) {
      return static_cast<int>(i);
    }
  }
  return -1;
}

}  // namespace eng::editor
