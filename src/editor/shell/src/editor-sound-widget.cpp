#include <algorithm>
#include <editor/shell/editor-sound-widget.h>
#include <engine/gui/gui-draw-context.h>
#include <engine/gui/gui-theme-constants.h>
#include <string_view>
#include <utility>

namespace eng::editor {

namespace {

  /// Laid over the viewport while the screen is open.
  constexpr GuiColor BACKDROP{0, 0, 0, 150};
  /// The panel's width, and how much of its height is title and hint.
  constexpr float PANEL_W = 620.0f;
  constexpr float HEADER_H = 56.0f;
  constexpr float FOOTER_H = 36.0f;
  /// One row's height, and where its name and value start.
  constexpr float ROW_H = 28.0f;
  constexpr float NAME_X = 16.0f;
  constexpr float HEADING_X = 8.0f;
  constexpr float VALUE_X = 200.0f;
  /// A volume bar: its height, and the room kept right of it for the
  /// percentage.
  constexpr float BAR_H = 8.0f;
  constexpr float PERCENT_W = 64.0f;
  /// How many rows one notch of the wheel scrolls.
  constexpr long WHEEL_ROWS = 3;
  /// The scroll thumb's width, and its gap from the panel's edge.
  constexpr float THUMB_W = 4.0f;
  constexpr float THUMB_GAP = 4.0f;

  constexpr std::string_view TITLE = "Sound";

  /// The hint under the rows for each kind of row highlighted, by
  /// `EditorSoundRowKind`.
  constexpr std::string_view HINTS[] = {
      "Esc to close",
      "Left and right to change  ·  M mutes  ·  Esc to close",
      "Enter to switch  ·  Esc to close",
      "Left and right choose a file  ·  Enter plays it  ·  Delete for "
      "built-in  ·  I imports  ·  Esc to close",
  };
  static_assert(std::size(HINTS) ==
                static_cast<size_t>(EditorSoundRowKind::SLOT) + 1);

  /// The part of @p row from @p x in.
  Rect column(const Rect& row, float x) {
    return makeRect(row.x + x, row.y, row.w - x, row.h);
  }

}  // namespace

EditorSoundWidget::EditorSoundWidget() {
  widget_type = GuiWidgetType::PANEL;
  debug_name = "editor-sound";
  fill_color = THEME_PANEL;
  border_color = THEME_BORDER;
  border_width = 1.0f;
  visible = false;
}

std::unique_ptr<GuiWidget> EditorSoundWidget::clone() const {
  return std::make_unique<EditorSoundWidget>(*this);
}

void EditorSoundWidget::open(std::vector<EditorSoundRow> rows) {
  rows_ = std::move(rows);
  highlighted_ = selectable(0, 1);
  first_shown_ = 0;
  visible = isOpen();
}

void EditorSoundWidget::refresh(std::vector<EditorSoundRow> rows) {
  rows_ = std::move(rows);
  highlighted_ = std::min(highlighted_, rows_.empty() ? 0 : rows_.size() - 1);
  highlighted_ = selectable(highlighted_, 1);
  scrollTo(static_cast<long>(first_shown_));
  reveal();
}

void EditorSoundWidget::close() {
  rows_.clear();
  highlighted_ = 0;
  first_shown_ = 0;
  visible = false;
}

size_t EditorSoundWidget::selectable(size_t from, int step) const {
  const auto n = static_cast<long>(rows_.size());
  long at = static_cast<long>(from);
  for (long tried = 0; tried < n; ++tried) {
    if (rows_[static_cast<size_t>(at)].kind != EditorSoundRowKind::HEADING) {
      return static_cast<size_t>(at);
    }
    at = ((at + step) % n + n) % n;
  }
  return from;
}

void EditorSoundWidget::moveHighlight(int steps) {
  if (rows_.empty()) {
    return;
  }
  const auto n = static_cast<long>(rows_.size());
  const int step = steps < 0 ? -1 : 1;
  for (int moved = 0; moved != steps; moved += step) {
    const long next = ((static_cast<long>(highlighted_) + step) % n + n) % n;
    highlighted_ = selectable(static_cast<size_t>(next), step);
  }
  reveal();
}

size_t EditorSoundWidget::rowsFitting() const {
  const float room = std::max(0.0f, rect.h - HEADER_H - FOOTER_H);
  return std::max<size_t>(1, static_cast<size_t>(room / ROW_H));
}

void EditorSoundWidget::scrollTo(long first) {
  const auto last =
      static_cast<long>(rows_.size()) - static_cast<long>(rowsFitting());
  first_shown_ = static_cast<size_t>(std::clamp(first, 0L, std::max(0L, last)));
}

void EditorSoundWidget::reveal() {
  const size_t fitting = rowsFitting();
  if (highlighted_ < first_shown_) {
    scrollTo(static_cast<long>(highlighted_));
  } else if (highlighted_ >= first_shown_ + fitting) {
    scrollTo(static_cast<long>(highlighted_ + 1 - fitting));
  }
  // A heading just above the first row in view is shown with it, so the
  // top of a group is never a row without its title.
  if (first_shown_ > 0 && first_shown_ == highlighted_ &&
      rows_[first_shown_ - 1].kind == EditorSoundRowKind::HEADING) {
    scrollTo(static_cast<long>(first_shown_) - 1);
  }
}

bool EditorSoundWidget::rowShown(size_t index) const {
  return index >= first_shown_ && index < first_shown_ + rowsFitting();
}

Rect EditorSoundWidget::panelRect() const {
  const size_t shown = std::min(rows_.size(), rowsFitting());
  const float h = HEADER_H + FOOTER_H + ROW_H * static_cast<float>(shown);
  const float w = std::min(PANEL_W, rect.w);
  return makeRect(rect.x + (rect.w - w) * 0.5f,
                  rect.y + std::max(0.0f, (rect.h - h) * 0.5f), w, h);
}

Rect EditorSoundWidget::rowRect(size_t index) const {
  const Rect panel = panelRect();
  return makeRect(panel.x,
                  panel.y + HEADER_H +
                      ROW_H * (static_cast<float>(index) -
                               static_cast<float>(first_shown_)),
                  panel.w, ROW_H);
}

Rect EditorSoundWidget::barRect(size_t index) const {
  const Rect row = rowRect(index);
  const float w = std::max(0.0f, row.w - VALUE_X - PERCENT_W);
  return makeRect(row.x + VALUE_X, row.y + (row.h - BAR_H) * 0.5f, w, BAR_H);
}

void EditorSoundWidget::renderBar(const GuiDrawContext& ctx,
                                  size_t index) const {
  const Rect bar = barRect(index);
  const float level = std::clamp(rows_[index].level, 0.0f, 1.0f);
  ctx.drawRoundedRect(bar, GuiColor::applyOpacity(THEME_BORDER, opacity),
                      BAR_H * 0.5f);
  ctx.drawRoundedRect(makeRect(bar.x, bar.y, bar.w * level, bar.h),
                      GuiColor::applyOpacity(THEME_TEXT, opacity),
                      BAR_H * 0.5f);
  const Rect row = rowRect(index);
  const Rect percent =
      makeRect(bar.x + bar.w + 12.0f, row.y, PERCENT_W - 12.0f, row.h);
  ctx.drawText(GuiColor::applyOpacity(THEME_DIM, opacity),
               drawPosInset(percent, 0.0f, 4.0f), rows_[index].value);
}

void EditorSoundWidget::renderRow(const GuiDrawContext& ctx,
                                  size_t index) const {
  if (rows_[index].kind == EditorSoundRowKind::HEADING) {
    ctx.drawText(GuiColor::applyOpacity(THEME_DIM, opacity),
                 drawPosInset(column(rowRect(index), HEADING_X), 0.0f, 4.0f),
                 rows_[index].name);
    return;
  }
  renderEntry(ctx, index);
}

void EditorSoundWidget::renderEntry(const GuiDrawContext& ctx,
                                    size_t index) const {
  const Rect row = rowRect(index);
  const EditorSoundRow& shown = rows_[index];
  const GuiColor text = GuiColor::applyOpacity(THEME_TEXT, opacity);
  const GuiColor dim = GuiColor::applyOpacity(THEME_DIM, opacity);
  if (index == highlighted_) {
    ctx.drawFilledRect(row, GuiColor::applyOpacity(THEME_BTN, opacity));
  }
  ctx.drawText(text, drawPosInset(column(row, NAME_X), 0.0f, 4.0f), shown.name);
  if (shown.kind == EditorSoundRowKind::VOLUME) {
    renderBar(ctx, index);
    return;
  }
  ctx.drawText(dim, drawPosInset(column(row, VALUE_X), 0.0f, 4.0f),
               shown.value);
}

void EditorSoundWidget::render(const GuiDrawContext& ctx) const {
  if (!isOpen() || rect.w <= 0.0f || rect.h <= 0.0f) {
    return;
  }
  const Rect panel = panelRect();
  ctx.drawFilledRect(rect, GuiColor::applyOpacity(BACKDROP, opacity));
  ctx.drawRoundedRect(panel, GuiColor::applyOpacity(fill_color, opacity),
                      THEME_BTN_RADIUS);
  ctx.drawCenteredText(makeRect(panel.x, panel.y, panel.w, HEADER_H),
                       GuiColor::applyOpacity(THEME_TEXT, opacity), TITLE);
  renderRows(ctx);
  ctx.drawCenteredText(
      makeRect(panel.x, panel.y + panel.h - FOOTER_H, panel.w, FOOTER_H),
      GuiColor::applyOpacity(THEME_DIM, opacity), hint());
}

void EditorSoundWidget::renderRows(const GuiDrawContext& ctx) const {
  const size_t end = std::min(rows_.size(), first_shown_ + rowsFitting());
  for (size_t i = first_shown_; i < end; ++i) {
    renderRow(ctx, i);
  }
  renderScrollbar(ctx);
}

void EditorSoundWidget::renderScrollbar(const GuiDrawContext& ctx) const {
  const size_t fitting = rowsFitting();
  if (rows_.size() <= fitting) {
    return;
  }
  const Rect panel = panelRect();
  const float track = ROW_H * static_cast<float>(fitting);
  const auto total = static_cast<float>(rows_.size());
  const Rect thumb = makeRect(
      panel.x + panel.w - THUMB_W - THUMB_GAP,
      panel.y + HEADER_H + track * static_cast<float>(first_shown_) / total,
      THUMB_W, track * static_cast<float>(fitting) / total);
  ctx.drawRoundedRect(thumb, GuiColor::applyOpacity(THEME_DIM, opacity),
                      THUMB_W * 0.5f);
}

bool EditorSoundWidget::handleScroll(const GuiScrollEvent& event) {
  if (!isOpen()) {
    return false;
  }
  const long rows = event.delta_y > 0.0f ? -WHEEL_ROWS : WHEEL_ROWS;
  scrollTo(static_cast<long>(first_shown_) + rows);
  return true;
}

std::string EditorSoundWidget::hint() const {
  return std::string{HINTS[static_cast<size_t>(rows_[highlighted_].kind)]};
}

void EditorSoundWidget::clickRow(size_t index, float x) {
  highlighted_ = index;
  const Rect bar = barRect(index);
  if (rows_[index].kind == EditorSoundRowKind::VOLUME && bar.w > 0.0f &&
      x >= bar.x && on_level_picked) {
    on_level_picked(index, std::clamp((x - bar.x) / bar.w, 0.0f, 1.0f));
  }
}

bool EditorSoundWidget::handleMouseDown(const GuiMouseEvent& event) {
  if (!isOpen() || event.button != GuiMouseButton::LEFT) {
    return false;
  }
  for (size_t i = 0; i < rows_.size(); ++i) {
    if (rows_[i].kind != EditorSoundRowKind::HEADING && rowShown(i) &&
        containsPoint(rowRect(i), event.x, event.y)) {
      clickRow(i, event.x);
      return false;
    }
  }
  if (!containsPoint(panelRect(), event.x, event.y) && on_dismissed) {
    on_dismissed();
  }
  return false;
}

}  // namespace eng::editor
