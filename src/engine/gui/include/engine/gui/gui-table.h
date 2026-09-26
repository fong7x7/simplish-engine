#pragma once

/// @file gui-table.h
/// @brief A virtual list with columns: headers to sort by, widths to drag.
/// @par Threading
/// Main thread only.

#include "gui-sort-direction.h"
#include "gui-table-column.h"
#include "gui-text-draw.h"
#include "gui-virtual-list.h"

#include <functional>
#include <string>
#include <vector>

namespace eng {

/// A table: `GuiVirtualList` rows under a header of `columns`. Each cell
/// shows `cell_text(row, column)`, aligned as its column says; a click on
/// a sortable header asks `on_sort` to reorder the data (the table shows
/// the arrow, the data is the caller's); dragging a header's right edge
/// resizes the column. Selection, scrolling and keys are the list's.
///
/// ```cpp
/// table->columns = {{"Name", 200}, {"Health", 80, Align::END}};
/// table->row_count = actors.size();
/// table->cell_text = [&](size_t r, size_t c) {
///   return c == 0 ? actors[r].name : std::to_string(actors[r].health);
/// };
/// table->on_sort = [&](size_t c, GuiSortDirection d) { sortActors(c, d); };
/// ```
/// @thread_safety Main thread only.
class GuiTable : public GuiVirtualList {
public:
  /// Polymorphic deep-copy.
  [[nodiscard]] std::unique_ptr<GuiWidget> clone() const override;

  /// The header, then the rows.
  void render(const GuiDrawContext& ctx) const override;

  /// Its rect below the header.
  [[nodiscard]] Rect bodyRect() const override;

  /// A press on a header's right edge starts resizing that column.
  bool handleMouseDown(const GuiMouseEvent& event) override;

  /// Resize the column being dragged; else as the list.
  void handleMouseMove(const GuiMouseEvent& event) override;

  /// End a resize.
  void handleMouseUp(const GuiMouseEvent& event) override;

  /// A header click sorts by that column; a row click is the list's.
  bool handleClick(const GuiMouseEvent& event) override;

  /// The column whose header is at @p x, or -1.
  [[nodiscard]] long columnAt(float x) const;

  /// Its columns, left to right.
  std::vector<GuiTableColumn> columns{};
  /// The text in a cell.
  std::function<std::string(std::size_t row, std::size_t column)> cell_text{};
  /// Called when a header asks for a sort, with the column and direction.
  std::function<void(std::size_t, GuiSortDirection)> on_sort{};
  /// The column sorted by, shown with an arrow; -1 for none.
  long sort_column = -1;
  /// Which way it is sorted.
  GuiSortDirection sort_direction = GuiSortDirection::ASCENDING;
  /// Narrowest a column can be dragged.
  float min_column_width = 40.0f;

protected:
  /// A row's cells.
  void drawRow(const GuiDrawContext& ctx, const GuiListRow& row) const override;

private:
  /// The header bar.
  void drawHeader(const GuiDrawContext& ctx) const;
  /// Column @p column's header, with the sort arrow if it is sorted by.
  [[nodiscard]] std::string headerText(std::size_t column) const;

  /// The column whose right edge is at @p x, give or take a few pixels, or
  /// -1.
  [[nodiscard]] long edgeAt(float x) const;

  /// The column being resized, or -1.
  long resizing_ = -1;
};

}  // namespace eng
