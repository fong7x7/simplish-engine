#pragma once

/// @file gui-virtual-list.h
/// @brief A scrolling list of any length that draws only the rows in view.
/// @par Threading
/// Main thread only.

#include "gui-list-row.h"
#include "gui-list-selection.h"
#include "gui-widget.h"

#include <functional>
#include <vector>

namespace eng {

/// A virtual list: `row_count` rows of `row_height`, of which it draws only
/// those in view — ten thousand rows cost what twenty do — through
/// `draw_row`, which paints one row's content into its rect. The list
/// draws the row backgrounds (hover, selection, the current row), scrolls
/// with the wheel and a thumb, selects by click (shift-click extends when
/// MULTIPLE), and moves the current row with UP and DOWN, keeping it in
/// view; CONFIRM or a double-click activates it.
///
/// ```cpp
/// list->row_count = entries.size();
/// list->draw_row = [&](const GuiDrawContext& ctx, const GuiListRow& row) {
///   ctx.drawText({.text = entries[row.index].name,
///                 .pos = {row.rect.x + 8, row.rect.y + 5}, ...});
/// };
/// list->on_activate = [&](size_t i) { open(entries[i]); };
/// ```
/// @thread_safety Main thread only.
class GuiVirtualList : public GuiWidget {
public:
  /// An empty, focusable list.
  GuiVirtualList();

  /// Polymorphic deep-copy.
  [[nodiscard]] std::unique_ptr<GuiWidget> clone() const override;

  /// The rows in view, then the scroll thumb.
  void render(const GuiDrawContext& ctx) const override;

  /// Nothing of its own: a list is as big as its layout makes it.
  [[nodiscard]] LayoutSize measureContent(const GuiDrawContext& ctx,
                                          float max_width) const override;

  /// Select the row under the pointer.
  bool handleClick(const GuiMouseEvent& event) override;

  /// Light the row under the pointer.
  void handleMouseMove(const GuiMouseEvent& event) override;

  /// Scroll by the wheel.
  bool handleScroll(const GuiScrollEvent& event) override;

  /// UP and DOWN move the current row; CONFIRM activates it.
  bool handleNav(GuiNavCommand command) override;

  /// Scroll by (@p dx, @p dy) pixels: a pad's right stick.
  bool scrollBy(float dx, float dy) override;

  /// Where the rows are drawn: its rect, less anything a subclass draws
  /// above them (a table's header).
  [[nodiscard]] virtual Rect bodyRect() const;

  /// The row at (@p x, @p y), or -1.
  [[nodiscard]] long rowAt(float x, float y) const;

  /// Scroll so row @p index is in view.
  void reveal(std::size_t index);

  /// Make row @p index the current one, selecting it alone.
  void setCurrent(std::size_t index);

  /// Whether row @p index is selected.
  [[nodiscard]] bool isSelected(std::size_t index) const;

  /// How far it is scrolled, in pixels.
  [[nodiscard]] float scrollOffset() const;

  /// How many rows there are.
  std::size_t row_count = 0;
  /// Every row's height.
  float row_height = 28.0f;
  /// How many rows can be selected.
  GuiListSelection selection_mode = GuiListSelection::SINGLE;
  /// The selected rows, ascending.
  std::vector<std::size_t> selected{};
  /// Paints one row's content; the list has drawn its background.
  std::function<void(const GuiDrawContext&, const GuiListRow&)> draw_row{};
  /// Called with the selection after it changes.
  std::function<void(const std::vector<std::size_t>&)> on_select{};
  /// Called with a row double-clicked or CONFIRMed.
  std::function<void(std::size_t)> on_activate{};

protected:
  /// Draw the rows in view in @p body.
  void drawRows(const GuiDrawContext& ctx, const Rect& body) const;
  /// Draw one row: its background, then `draw_row`.
  virtual void drawRow(const GuiDrawContext& ctx, const GuiListRow& row) const;
  /// Select row @p index by a click, with shift extending when MULTIPLE.
  void clickRow(std::size_t index, const GuiMouseEvent& event);
  /// Keep the scroll within what there is to scroll.
  void clampScroll();

  /// The row navigation is on.
  std::size_t current_ = 0;
  /// The row the pointer is on, or -1.
  long hovered_row_ = -1;
  /// Scroll offset in pixels.
  float scroll_ = 0.0f;
  /// The row a shift-click extends from.
  std::size_t anchor_ = 0;
};

}  // namespace eng
