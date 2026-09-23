#include <algorithm>
#include <editor/shell/editor-controls-widget.h>
#include <engine/gui/gui-draw-context.h>
#include <engine/gui/gui-theme-constants.h>
#include <string>
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
  /// One row's height, and the columns its three texts start at.
  constexpr float ROW_H = 28.0f;
  constexpr float NAME_X = 16.0f;
  constexpr float KEYS_X = 170.0f;
  constexpr float PAD_X = 360.0f;

  constexpr std::string_view TITLE = "Controls";
  constexpr std::string_view BROWSE_HINT =
      "Enter to rebind  ·  Delete clears  ·  R resets all  ·  Esc to close";

  /// A column of @p row starting @p x in.
  Rect column(const Rect& row, float x) {
    return makeRect(row.x + x, row.y, row.w - x, row.h);
  }

}  // namespace

EditorControlsWidget::EditorControlsWidget() {
  widget_type = GuiWidgetType::PANEL;
  debug_name = "editor-controls";
  fill_color = THEME_PANEL;
  border_color = THEME_BORDER;
  border_width = 1.0f;
  visible = false;
}

std::unique_ptr<GuiWidget> EditorControlsWidget::clone() const {
  return std::make_unique<EditorControlsWidget>(*this);
}

void EditorControlsWidget::open(std::vector<EditorControlsRow> rows) {
  rows_ = std::move(rows);
  highlighted_ = 0;
  mode_ = EditorControlsMode::BROWSING;
  visible = isOpen();
}

void EditorControlsWidget::refresh(std::vector<EditorControlsRow> rows) {
  rows_ = std::move(rows);
  highlighted_ = std::min(highlighted_, rows_.empty() ? 0 : rows_.size() - 1);
  mode_ = EditorControlsMode::BROWSING;
}

void EditorControlsWidget::close() {
  rows_.clear();
  highlighted_ = 0;
  mode_ = EditorControlsMode::BROWSING;
  visible = false;
}

void EditorControlsWidget::moveHighlight(int steps) {
  if (rows_.empty() || mode_ == EditorControlsMode::LISTENING) {
    return;
  }
  const auto n = static_cast<long>(rows_.size());
  const long to = ((static_cast<long>(highlighted_) + steps) % n + n) % n;
  highlighted_ = static_cast<size_t>(to);
}

void EditorControlsWidget::listen() {
  if (isOpen()) {
    mode_ = EditorControlsMode::LISTENING;
  }
}

Rect EditorControlsWidget::panelRect() const {
  const float h =
      HEADER_H + FOOTER_H + ROW_H * static_cast<float>(rows_.size());
  const float w = std::min(PANEL_W, rect.w);
  return makeRect(rect.x + (rect.w - w) * 0.5f,
                  rect.y + std::max(0.0f, (rect.h - h) * 0.5f), w, h);
}

Rect EditorControlsWidget::rowRect(size_t index) const {
  const Rect panel = panelRect();
  return makeRect(panel.x,
                  panel.y + HEADER_H + ROW_H * static_cast<float>(index),
                  panel.w, ROW_H);
}

void EditorControlsWidget::renderRow(const GuiDrawContext& ctx,
                                     size_t index) const {
  const Rect row = rowRect(index);
  const EditorControlsRow& shown = rows_[index];
  const bool lit = index == highlighted_;
  if (lit) {
    ctx.drawFilledRect(row, GuiColor::applyOpacity(THEME_BTN, opacity));
  }
  const bool listening = lit && mode_ == EditorControlsMode::LISTENING;
  const GuiColor text = GuiColor::applyOpacity(THEME_TEXT, opacity);
  const GuiColor dim = GuiColor::applyOpacity(THEME_DIM, opacity);
  ctx.drawText(text, drawPosInset(column(row, NAME_X), 0.0f, 4.0f), shown.name);
  ctx.drawText(listening ? text : dim,
               drawPosInset(column(row, KEYS_X), 0.0f, 4.0f),
               listening ? "Press a key or pad control…" : shown.keys);
  if (!listening) {
    ctx.drawText(dim, drawPosInset(column(row, PAD_X), 0.0f, 4.0f), shown.pad);
  }
}

void EditorControlsWidget::render(const GuiDrawContext& ctx) const {
  if (!isOpen() || rect.w <= 0.0f || rect.h <= 0.0f) {
    return;
  }
  const Rect panel = panelRect();
  ctx.drawFilledRect(rect, GuiColor::applyOpacity(BACKDROP, opacity));
  ctx.drawRoundedRect(panel, GuiColor::applyOpacity(fill_color, opacity),
                      THEME_BTN_RADIUS);
  ctx.drawCenteredText(makeRect(panel.x, panel.y, panel.w, HEADER_H),
                       GuiColor::applyOpacity(THEME_TEXT, opacity), TITLE);
  for (size_t i = 0; i < rows_.size(); ++i) {
    renderRow(ctx, i);
  }
  ctx.drawCenteredText(
      makeRect(panel.x, panel.y + panel.h - FOOTER_H, panel.w, FOOTER_H),
      GuiColor::applyOpacity(THEME_DIM, opacity), hint());
}

std::string EditorControlsWidget::hint() const {
  return mode_ == EditorControlsMode::LISTENING
             ? "Press the new control for " + rows_[highlighted_].name +
                   "  ·  Esc to stop"
             : std::string{BROWSE_HINT};
}

bool EditorControlsWidget::handleMouseDown(const GuiMouseEvent& event) {
  if (!isOpen() || event.button != GuiMouseButton::LEFT) {
    return false;
  }
  for (size_t i = 0; i < rows_.size(); ++i) {
    if (containsPoint(rowRect(i), event.x, event.y)) {
      highlighted_ = i;
      listen();
      return false;
    }
  }
  if (!containsPoint(panelRect(), event.x, event.y) && on_dismissed) {
    on_dismissed();
  }
  return false;
}

}  // namespace eng::editor
