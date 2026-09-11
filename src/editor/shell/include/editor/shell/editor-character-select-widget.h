#pragma once

/// @file editor-character-select-widget.h
/// @brief The screen a player picks their character on before a playtest.
/// @par Threading Main-thread-only.
///
/// Behaviours:
///   - Covers the viewport while open, dimming the level behind it, with a
///     panel of cards in the middle: one per character, each with its
///     model's picture, its name, and its speed and health
///   - One card is highlighted — the one the level's start for player 1
///     names, or the first — and the arrow keys move the highlight, which
///     the editor routes here
///   - A click on a card plays as that character; Enter plays as the
///     highlighted one. A click outside the panel, or Esc, cancels
///   - Closed, it is hidden and takes no input
///
/// Invariants:
///   - Drawing and hit testing both come from
///     `layoutEditorCharacterSelect`, so what is drawn is what is clicked
///   - The widget never starts anything itself: it reports a pick or a
///     cancel, and the editor decides what that means

#include <cstddef>
#include <editor/shell/editor-character-card.h>
#include <editor/shell/editor-character-select-layout.h>
#include <engine/gui/gui-panel.h>
#include <functional>
#include <memory>
#include <vector>

namespace eng::editor {

/// The character selector, drawn over the viewport.
/// @thread_safety Main-thread only.
class EditorCharacterSelectWidget : public GuiPanel {
public:
  EditorCharacterSelectWidget();

  /// Polymorphic deep-copy.
  [[nodiscard]] std::unique_ptr<GuiWidget> clone() const override;

  /// Show @p cards, highlighting the one at @p highlighted — or the first,
  /// when that is out of range.
  void open(std::vector<EditorCharacterCard> cards, size_t highlighted);

  /// Hide the selector, dropping its cards.
  void close();

  /// Whether the selector is showing.
  [[nodiscard]] bool isOpen() const { return !cards_.empty(); }

  /// The card highlighted, as an index into the list `open` was given.
  [[nodiscard]] size_t highlighted() const { return highlighted_; }

  /// The cards showing, in order.
  [[nodiscard]] const std::vector<EditorCharacterCard>& cards() const {
    return cards_;
  }

  /// Move the highlight @p steps cards along, wrapping round at either end.
  void moveHighlight(int steps);

  /// Report the highlighted card as picked.
  void confirm() const;

  /// Report the selector as dismissed.
  void cancel() const;

  /// Where the selector's parts sit for its current rect.
  [[nodiscard]] EditorCharacterSelectLayout layout() const;

  /// Draw the dimmed backdrop, the panel, and every card.
  void render(const GuiDrawContext& ctx) const override;

  /// Pick the card clicked, or cancel on a click off the panel. Never
  /// captures: a pick is done the moment it is pressed.
  bool handleMouseDown(const GuiMouseEvent& event) override;

  /// Raised with the index of the card picked.
  std::function<void(size_t)> on_chosen{};

  /// Raised when the selector is dismissed without a pick.
  std::function<void()> on_cancelled{};

private:
  /// Draw card @p index in @p card.
  void renderCard(const GuiDrawContext& ctx, const Rect& card,
                  size_t index) const;
  /// Draw the picture well of @p card, and the picture in it when there is
  /// one.
  void renderPicture(const GuiDrawContext& ctx, const Rect& card,
                     size_t index) const;

  /// The cards showing; empty while closed.
  std::vector<EditorCharacterCard> cards_{};
  /// Which of them is highlighted.
  size_t highlighted_ = 0;
};

}  // namespace eng::editor
