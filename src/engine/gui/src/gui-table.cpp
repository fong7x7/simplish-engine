#include <algorithm>
#include <cmath>
#include <engine/gui/gui-draw-context.h>
#include <engine/gui/gui-table.h>
#include <utility>

namespace eng {

namespace {

  /// Padding inside a cell, and how near a header edge a press resizes.
  constexpr float CELL_PAD = 8.0f;
  constexpr float EDGE_GRAB = 4.0f;
  /// ▲ and ▼.
  constexpr std::string_view UP_ARROW = "\xE2\x96\xB2";
  constexpr std::string_view DOWN_ARROW = "\xE2\x96\xBC";

  /// What drawing one cell needs.
  struct Cell {
    /// Its rect.
    Rect rect;
    /// Its text.
    const std::string& text;
    /// How it aligns.
    Align align = Align::START;
  };

  void drawCell(const GuiDrawContext& ctx, const Cell& cell,
                const GuiTextDraw& style) {
    const GuiFont& font = style.font;
    const float room = cell.rect.w - CELL_PAD * 2.0f;
    const std::string shown = ctx.ellipsize(cell.text, font, room);
    const float w = ctx.measureText(shown, font);
    float x = cell.rect.x + CELL_PAD;
    if (cell.align == Align::END) {
      x = cell.rect.x + cell.rect.w - CELL_PAD - w;
    } else if (cell.align == Align::CENTER) {
      x = cell.rect.x + (cell.rect.w - w) * 0.5f;
    }
    const float lh = ctx.fontMetrics(font).line_height;
    GuiTextDraw draw = style;
    draw.text = shown;
    draw.pos = {x, cell.rect.y + (cell.rect.h - lh) * 0.5f};
    ctx.drawText(draw);
  }

  /// A row of cells to draw: where, how, and each column's text.
  struct CellBand {
    /// The band across the table.
    Rect rect;
    /// How the text is set.
    GuiTextDraw style;
    /// Each column's text.
    std::function<std::string(std::size_t)> text;
  };

  /// Draw a cell per one of @p columns across @p band.
  void drawCells(const GuiDrawContext& ctx,
                 const std::vector<GuiTableColumn>& columns,
                 const CellBand& band) {
    float x = band.rect.x;
    for (std::size_t c = 0; c < columns.size(); ++c) {
      const std::string cell = band.text(c);
      drawCell(ctx,
               {{x, band.rect.y, columns[c].width, band.rect.h},
                cell,
                columns[c].align},
               band.style);
      x += columns[c].width;
    }
  }

}  // namespace

std::unique_ptr<GuiWidget> GuiTable::clone() const {
  return std::make_unique<GuiTable>(*this);
}

Rect GuiTable::bodyRect() const {
  return {rect.x, rect.y + row_height, rect.w,
          std::max(0.0f, rect.h - row_height)};
}

void GuiTable::render(const GuiDrawContext& ctx) const {
  drawHeader(ctx);
  GuiVirtualList::render(ctx);
}

void GuiTable::drawHeader(const GuiDrawContext& ctx) const {
  const GuiTheme& t = ctx.activeTheme();
  ctx.drawRect(
      {.rect = {rect.x, rect.y, rect.w, row_height},
       .fill = GuiColor::applyOpacity(t.palette.surface, opacity),
       .border = {0.0f, 0.0f, 1.0f, 0.0f},
       .border_color = GuiColor::applyOpacity(t.palette.border, opacity)});
  const GuiTextDraw style{
      .color = GuiColor::applyOpacity(t.palette.text_muted, opacity),
      .font = t.font(GuiTextRole::LABEL)};
  drawCells(ctx, columns,
            {{rect.x, rect.y, rect.w, row_height},
             style,
             [this](std::size_t c) { return headerText(c); }});
}

std::string GuiTable::headerText(std::size_t column) const {
  if (std::cmp_not_equal(column, sort_column)) {
    return columns[column].title;
  }
  const std::string_view arrow =
      sort_direction == GuiSortDirection::ASCENDING ? UP_ARROW : DOWN_ARROW;
  return columns[column].title + " " + std::string(arrow);
}


void GuiTable::drawRow(const GuiDrawContext& ctx, const GuiListRow& row) const {
  GuiVirtualList::drawRow(ctx, row);
  if (!cell_text) {
    return;
  }
  const GuiTheme& t = ctx.activeTheme();
  const GuiTextDraw style{.color =
                              GuiColor::applyOpacity(t.palette.text, opacity),
                          .font = t.font(GuiTextRole::BODY)};
  drawCells(ctx, columns, {row.rect, style, [&](std::size_t c) {
                             return cell_text(row.index, c);
                           }});
}

long GuiTable::columnAt(float x) const {
  float left = rect.x;
  for (std::size_t c = 0; c < columns.size(); ++c) {
    if (x >= left && x < left + columns[c].width) {
      return static_cast<long>(c);
    }
    left += columns[c].width;
  }
  return -1;
}

long GuiTable::edgeAt(float x) const {
  float right = rect.x;
  for (std::size_t c = 0; c < columns.size(); ++c) {
    right += columns[c].width;
    if (std::abs(x - right) <= EDGE_GRAB) {
      return static_cast<long>(c);
    }
  }
  return -1;
}

bool GuiTable::handleMouseDown(const GuiMouseEvent& event) {
  const bool header = event.y < rect.y + row_height;
  resizing_ = header ? edgeAt(event.x) : -1;
  return resizing_ >= 0;
}

void GuiTable::handleMouseMove(const GuiMouseEvent& event) {
  if (resizing_ < 0) {
    GuiVirtualList::handleMouseMove(event);
    return;
  }
  float left = rect.x;
  for (long c = 0; c < resizing_; ++c) {
    left += columns[static_cast<std::size_t>(c)].width;
  }
  columns[static_cast<std::size_t>(resizing_)].width =
      std::max(min_column_width, event.x - left);
}

void GuiTable::handleMouseUp(const GuiMouseEvent& event) {
  resizing_ = -1;
  GuiVirtualList::handleMouseUp(event);
}

bool GuiTable::handleClick(const GuiMouseEvent& event) {
  if (event.y >= rect.y + row_height) {
    return GuiVirtualList::handleClick(event);
  }
  const long c = columnAt(event.x);
  if (c < 0 || !columns[static_cast<std::size_t>(c)].sortable) {
    return false;
  }
  sort_direction =
      c == sort_column && sort_direction == GuiSortDirection::ASCENDING
          ? GuiSortDirection::DESCENDING
          : GuiSortDirection::ASCENDING;
  sort_column = c;
  if (on_sort) {
    on_sort(static_cast<std::size_t>(c), sort_direction);
  }
  return true;
}

}  // namespace eng
