#include "engine/gui/gui-scroll-panel.h"

#include "engine/gui/gui-draw-context.h"
#include "engine/gui/gui-style.h"
#include "engine/gui/gui-widget-tree.h"

#include <algorithm>

namespace eng {

namespace {

  /// Width of the scroll thumb, and its gap from the edge.
  constexpr float THUMB_WIDTH = 4.0f;
  constexpr float THUMB_INSET = 2.0f;
  /// The shortest the thumb gets, so it stays something to see.
  constexpr float THUMB_MIN = 16.0f;

  /// The height @p child takes in the column.
  float heightOf(const GuiWidget& child, float row_height) {
    return child.tree_layout.height > 0.0f ? child.tree_layout.height
                                           : row_height;
  }

}  // namespace

GuiScrollPanel::GuiScrollPanel() {
  widget_type = GuiWidgetType::SCROLL_CONTAINER;
}

std::unique_ptr<GuiWidget> GuiScrollPanel::clone() const {
  return std::make_unique<GuiScrollPanel>(*this);
}

Rect GuiScrollPanel::viewport() const {
  const Edges& pad = tree_layout.padding;
  return {rect.x + pad.left, rect.y + pad.top,
          std::max(0.0f, rect.w - pad.left - pad.right),
          std::max(0.0f, rect.h - pad.top - pad.bottom)};
}

float GuiScrollPanel::maxScroll() const {
  return std::max(0.0f, scroll_.content_h - viewport_h_);
}

void GuiScrollPanel::setScrollOffset(float offset) {
  scroll_.offset_y = std::clamp(offset, 0.0f, maxScroll());
}

void GuiScrollPanel::arrangeChildren(GuiWidgetTree& tree,
                                     const Rect& available) {
  rect = available;
  const Rect view = viewport();
  float y = 0.0f;
  for (const GuiWidgetId id : children) {
    const GuiWidget* child = tree.findWidget(id);
    if (child == nullptr || !child->visible) {
      continue;
    }
    const float h = heightOf(*child, row_height);
    tree.arrangeWidget(id, {view.x, view.y + y - scroll_.offset_y, view.w, h});
    y += h + tree_layout.gap;
  }
  scroll_.content_h = std::max(0.0f, y - tree_layout.gap);
  viewport_h_ = view.h;
  setScrollOffset(scroll_.offset_y);
}

bool GuiScrollPanel::handleScroll(const GuiScrollEvent& event) {
  (void)GuiPanel::handleScroll(event);
  const float was = scroll_.offset_y;
  setScrollOffset(was - event.delta_y * wheel_step);
  return scroll_.offset_y != was;
}

bool GuiScrollPanel::revealChild(const Rect& child) {
  const Rect view = viewport();
  const float was = scroll_.offset_y;
  if (child.y < view.y) {
    setScrollOffset(was - (view.y - child.y));
  } else if (child.y + child.h > view.y + view.h) {
    // A child taller than the view shows its top rather than its bottom.
    const float over = child.y + child.h - (view.y + view.h);
    setScrollOffset(was + std::min(over, child.y - view.y));
  }
  return scroll_.offset_y != was;
}

bool GuiScrollPanel::scrollByNav(GuiNavCommand command) {
  if (command != GuiNavCommand::UP && command != GuiNavCommand::DOWN) {
    return false;
  }
  const float was = scroll_.offset_y;
  setScrollOffset(was +
                  (command == GuiNavCommand::DOWN ? nav_step : -nav_step));
  return scroll_.offset_y != was;
}

void GuiScrollPanel::arrangeAfterScroll(GuiWidgetTree& tree) {
  arrangeChildren(tree, rect);
}

std::optional<Rect> GuiScrollPanel::childClipRect() const {
  return viewport();
}

void GuiScrollPanel::render(const GuiDrawContext& ctx) const {
  GuiPanel::render(ctx);
  if (maxScroll() <= 0.0f || scroll_.content_h <= 0.0f) {
    return;
  }
  const Rect view = viewport();
  const float thumb_h =
      std::max(THUMB_MIN, view.h * viewport_h_ / scroll_.content_h);
  const float travel = std::max(0.0f, view.h - thumb_h);
  const Rect thumb{rect.x + rect.w - THUMB_WIDTH - THUMB_INSET,
                   view.y + travel * (scroll_.offset_y / maxScroll()),
                   THUMB_WIDTH, thumb_h};
  const GuiStyle& style = hasSharedStyle() ? *ui_style : GuiStyle::dark();
  ctx.drawRoundedRect(thumb, GuiColor::applyOpacity(style.text_dim, opacity),
                      THUMB_WIDTH * 0.5f);
}

}  // namespace eng
