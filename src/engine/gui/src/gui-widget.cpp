#include "engine/gui/gui-widget.h"

#include "engine/gui/gui-widget-tree.h"

#include <cstddef>

namespace eng {

namespace {

  /// Compute the rect for a child at `index` in a uniform column layout of
  /// `child_count` slices inside `available`.
  Rect uniformColumnSlice(const Rect& available, size_t child_count,
                          size_t index) {
    float child_h = available.h / static_cast<float>(child_count);
    return {available.x, available.y + child_h * static_cast<float>(index),
            available.w, child_h};
  }

}  // namespace

void GuiWidget::resetTransientState() {
  hovered = false;
  pressed = false;
  tree_dirty = true;
  tree_render_dirty = true;
}

void GuiWidget::update(const GuiDrawContext& /*ctx*/, float dt) {
  animator_.tick(*this, dt);
}

bool GuiWidget::isInside(float mx, float my) const {
  return visible && containsPoint(rect, mx, my);
}

void GuiWidget::onMouseDown(
    const std::function<void(const GuiMouseEvent&)>& handler) {
  on_mouse_down_handlers_.emplace_back(handler);
}

void GuiWidget::onMouseUp(
    const std::function<void(const GuiMouseEvent&)>& handler) {
  on_mouse_up_handlers_.emplace_back(handler);
}

void GuiWidget::onMouseMove(
    const std::function<void(const GuiMouseEvent&)>& handler) {
  on_mouse_move_handlers_.emplace_back(handler);
}

void GuiWidget::onScroll(
    const std::function<void(const GuiScrollEvent&)>& handler) {
  on_scroll_handlers_.emplace_back(handler);
}

void GuiWidget::onClick(
    const std::function<void(const GuiMouseEvent&)>& handler) {
  on_click_handlers_.emplace_back(handler);
}

void GuiWidget::clearClickHandlers() {
  on_click_handlers_.clear();
}

bool GuiWidget::hasClickHandlers() const {
  return !on_click_handlers_.empty();
}

bool GuiWidget::handleMouseDown(const GuiMouseEvent& event) {
  for (const auto& handler : on_mouse_down_handlers_) {
    handler(event);
  }
  return false;
}

void GuiWidget::handleMouseUp(const GuiMouseEvent& event) {
  for (const auto& handler : on_mouse_up_handlers_) {
    handler(event);
  }
}

void GuiWidget::handleMouseMove(const GuiMouseEvent& event) {
  for (const auto& handler : on_mouse_move_handlers_) {
    handler(event);
  }
}

bool GuiWidget::handleScroll(const GuiScrollEvent& event) {
  for (const auto& handler : on_scroll_handlers_) {
    handler(event);
  }
  return false;
}

bool GuiWidget::handleClick(const GuiMouseEvent& event) {
  for (const auto& handler : on_click_handlers_) {
    handler(event);
  }
  return false;
}

bool GuiWidget::hasSharedStyle() const {
  return ui_style != nullptr && !override_style;
}

void GuiWidget::animate(const GuiAnimation& anim) {
  animator_.add(anim);
}

void GuiWidget::fadeIn(float duration, GuiEasing easing) {
  opacity = 0.0f;
  animate({GuiAnimProperty::OPACITY,
           {.scalar = 0.0f},
           {.scalar = 1.0f},
           duration,
           0.0f,
           easing});
}

void GuiWidget::fadeOut(float duration, GuiEasing easing) {
  animate({GuiAnimProperty::OPACITY,
           {.scalar = opacity},
           {.scalar = 0.0f},
           duration,
           0.0f,
           easing});
}

/// Named algorithm: creates four concurrent rect property animations
/// (x, y, w, h) to slide the widget to a new position/size.
/// Pure animation setup — no side effects beyond adding animations.
void GuiWidget::slideTo(const Rect& target, float duration, GuiEasing easing) {
  animate({GuiAnimProperty::RECT_X,
           {.scalar = rect.x},
           {.scalar = target.x},
           duration,
           0.0f,
           easing});
  animate({GuiAnimProperty::RECT_Y,
           {.scalar = rect.y},
           {.scalar = target.y},
           duration,
           0.0f,
           easing});
  animate({GuiAnimProperty::RECT_W,
           {.scalar = rect.w},
           {.scalar = target.w},
           duration,
           0.0f,
           easing});
  animate({GuiAnimProperty::RECT_H,
           {.scalar = rect.h},
           {.scalar = target.h},
           duration,
           0.0f,
           easing});
}

void GuiWidget::cancelAnimations() {
  animator_.cancelAll();
}

bool GuiWidget::isAnimating() const {
  return animator_.hasActiveAnimations();
}

void GuiWidget::arrangeChildren(GuiWidgetTree& tree, const Rect& available) {
  if (children.empty()) {
    return;
  }
  for (size_t i = 0; i < children.size(); ++i) {
    auto* child = tree.findWidget(children[i]);
    if (child == nullptr) {
      continue;
    }
    child->rect = uniformColumnSlice(available, children.size(), i);
  }
}

}  // namespace eng
