#include <editor/shell/editor-asset-panel-widget.h>
#include <engine/gui/gui-color.h>
#include <engine/gui/gui-draw-context.h>
#include <engine/gui/gui-theme-constants.h>
#include <utility>

namespace eng::editor {

namespace {

  constexpr float HEADER_HEIGHT = 22.0f;
  constexpr float SIDE_PADDING = 10.0f;
  constexpr float CARD_WIDTH = 104.0f;
  constexpr float CARD_GAP = 8.0f;
  constexpr float CARD_INSET = 6.0f;
  constexpr float LABEL_INSET = 8.0f;
  constexpr float TEXT_DROP = 7.0f;

  constexpr GuiColor CARD_FILL{52, 52, 58, 255};
  constexpr GuiColor CARD_BORDER{72, 72, 80, 255};
  constexpr GuiColor GHOST_FILL{0, 122, 204, 150};

  /// Top of the card row within a panel rect.
  float cardTop(const Rect& panel) {
    return panel.y + HEADER_HEIGHT;
  }

  /// Height of a card within a panel rect.
  float cardHeight(const Rect& panel) {
    return panel.h - HEADER_HEIGHT - CARD_INSET;
  }

}  // namespace

EditorAssetPanelWidget::EditorAssetPanelWidget() {
  widget_type = GuiWidgetType::PANEL;
  debug_name = "editor-asset-panel";
  fill_color = THEME_PANEL;
  border_color = THEME_BORDER;
  border_width = 1.0f;
}

std::unique_ptr<GuiWidget> EditorAssetPanelWidget::clone() const {
  return std::make_unique<EditorAssetPanelWidget>(*this);
}

Rect EditorAssetPanelWidget::cardRect(size_t index) const {
  const float x = rect.x + SIDE_PADDING +
                  static_cast<float>(index) * (CARD_WIDTH + CARD_GAP);
  return makeRect(x, cardTop(rect), CARD_WIDTH, cardHeight(rect));
}

int EditorAssetPanelWidget::hitTestCard(float x, float y) const {
  if (!visible || names_.empty() || !containsPoint(rect, x, y)) {
    return -1;
  }
  for (size_t i = 0; i < names_.size(); ++i) {
    if (containsPoint(cardRect(i), x, y)) {
      return static_cast<int>(i);
    }
  }
  return -1;
}

void EditorAssetPanelWidget::setAssetNames(std::vector<std::string> names) {
  names_ = std::move(names);
  dragging_ = -1;
}

bool EditorAssetPanelWidget::handleMouseDown(const GuiMouseEvent& event) {
  if (event.button != GuiMouseButton::LEFT) {
    return false;
  }
  const int card = hitTestCard(event.x, event.y);
  if (card < 0) {
    return false;
  }
  dragging_ = card;
  drag_x_ = event.x;
  drag_y_ = event.y;
  // Capturing is what lets the drag continue over the viewport: without it
  // the move events would go to whatever the cursor is over instead.
  return true;
}

void EditorAssetPanelWidget::handleMouseMove(const GuiMouseEvent& event) {
  if (dragging_ < 0) {
    return;
  }
  drag_x_ = event.x;
  drag_y_ = event.y;
}

void EditorAssetPanelWidget::handleMouseUp(const GuiMouseEvent& event) {
  const int card = dragging_;
  dragging_ = -1;
  if (card < 0) {
    return;
  }
  // A release back over the panel is a cancelled drag, not a placement.
  if (containsPoint(rect, event.x, event.y) || !on_asset_dropped) {
    return;
  }
  on_asset_dropped(static_cast<size_t>(card), event.x, event.y);
}

void EditorAssetPanelWidget::renderHeader(const GuiDrawContext& ctx) const {
  const auto text_color = GuiColor::applyOpacity(THEME_DIM, opacity);
  ctx.drawText(text_color, drawPosInset(rect, SIDE_PADDING, TEXT_DROP),
               "Assets");
  if (!names_.empty()) {
    return;
  }
  const Rect empty_row{rect.x, cardTop(rect), rect.w, cardHeight(rect)};
  ctx.drawText(text_color, drawPosInset(empty_row, SIDE_PADDING, TEXT_DROP),
               "Drop .obj files into the project's assets/ folder");
}

void EditorAssetPanelWidget::renderCard(const GuiDrawContext& ctx,
                                        size_t index) const {
  const Rect card = cardRect(index);
  ctx.drawFilledRect(card, GuiColor::applyOpacity(CARD_FILL, opacity));
  ctx.drawBorderRect(card, GuiColor::applyOpacity(CARD_BORDER, opacity));
  ctx.drawText(GuiColor::applyOpacity(THEME_TEXT, opacity),
               drawPosInset(card, LABEL_INSET, LABEL_INSET), names_[index]);
}

void EditorAssetPanelWidget::renderCards(const GuiDrawContext& ctx) const {
  const float right_edge = rect.x + rect.w;
  for (size_t i = 0; i < names_.size(); ++i) {
    // Cards that start past the edge are dropped rather than drawn over the
    // border: there is no scrolling yet.
    if (cardRect(i).x >= right_edge) {
      return;
    }
    renderCard(ctx, i);
  }
}

void EditorAssetPanelWidget::renderDragGhost(const GuiDrawContext& ctx) const {
  if (dragging_ < 0) {
    return;
  }
  const Rect card = cardRect(static_cast<size_t>(dragging_));
  const Rect ghost{drag_x_ - card.w * 0.5f, drag_y_ - card.h * 0.5f, card.w,
                   card.h};
  ctx.drawFilledRect(ghost, GuiColor::applyOpacity(GHOST_FILL, opacity));
  ctx.drawText(GuiColor::applyOpacity(THEME_TEXT, opacity),
               drawPosInset(ghost, LABEL_INSET, LABEL_INSET),
               names_[static_cast<size_t>(dragging_)]);
}

void EditorAssetPanelWidget::render(const GuiDrawContext& ctx) const {
  if (rect.w <= 0.0f || rect.h <= 0.0f) {
    return;
  }
  renderPanel({ctx, GuiColor::applyOpacity(fill_color, opacity)});
  ctx.drawBorderRect(rect, GuiColor::applyOpacity(THEME_BORDER, opacity));
  renderHeader(ctx);
  renderCards(ctx);
  renderDragGhost(ctx);
}

}  // namespace eng::editor
