#include "engine/gui/gui-widget.h"

#include "engine/gui/gui-widget-tree.h"
#include "flex-layout.h"

#include <cstddef>

namespace eng {

void GuiWidget::resetTransientState() {
  hovered = false;
  pressed = false;
  tree_dirty = true;
  tree_render_dirty = true;
}

void GuiWidget::update(const GuiDrawContext& ctx, float dt) {
  const float step = guiMotionStep(ctx.motion, dt);
  animator_.tick(*this, step);
  const GuiTheme& theme = ctx.activeTheme();
  if (const GuiStateStyles* styles = activeStyles(theme)) {
    style_transition_.retarget(styles->of(visualState()),
                               theme.transition_seconds);
    style_transition_.tick(step);
  }
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
  if (disabled) {
    return false;
  }
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

bool GuiWidget::handleNav(GuiNavCommand command) {
  if (disabled || command != GuiNavCommand::CONFIRM || !hasClickHandlers()) {
    return false;
  }
  // Pressed as the pointer would press it, at its centre.
  handleClick({.x = rect.x + rect.w * 0.5f, .y = rect.y + rect.h * 0.5f});
  return true;
}

bool GuiWidget::revealChild(const Rect& /*child*/) {
  return false;
}

void GuiWidget::onFocusChange(
    const std::function<void(GuiFocusChange)>& handler) {
  on_focus_handlers_.emplace_back(handler);
}

void GuiWidget::handleFocusChange(GuiFocusChange change) {
  focused = change == GuiFocusChange::GAINED;
  for (const auto& handler : on_focus_handlers_) {
    handler(change);
  }
}

bool GuiWidget::scrollBy(float /*dx*/, float /*dy*/) {
  return false;
}

bool GuiWidget::scrollByNav(GuiNavCommand /*command*/) {
  return false;
}

void GuiWidget::arrangeAfterScroll(GuiWidgetTree& /*tree*/) {}

std::optional<Rect> GuiWidget::childClipRect() const {
  return std::nullopt;
}

bool GuiWidget::handleClick(const GuiMouseEvent& event) {
  if (disabled) {
    return false;
  }
  for (const auto& handler : on_click_handlers_) {
    handler(event);
  }
  return false;
}

GuiWidgetState GuiWidget::visualState() const {
  if (disabled) {
    return GuiWidgetState::DISABLED;
  }
  if (pressed) {
    return GuiWidgetState::PRESSED;
  }
  if (selected) {
    return GuiWidgetState::SELECTED;
  }
  if (hovered) {
    return GuiWidgetState::HOVER;
  }
  return focused ? GuiWidgetState::FOCUSED : GuiWidgetState::NORMAL;
}

const GuiStateStyles* GuiWidget::themeStyles(const GuiTheme& /*theme*/) const {
  return nullptr;
}

const GuiStateStyles* GuiWidget::activeStyles(const GuiTheme& theme) const {
  return state_styles ? &*state_styles : themeStyles(theme);
}

GuiStateStyle GuiWidget::drawnStyle(const GuiDrawContext& ctx) const {
  if (style_transition_.isBlending()) {
    return style_transition_.current();
  }
  const GuiStateStyles* styles = activeStyles(ctx.activeTheme());
  return styles != nullptr ? styles->of(visualState()) : GuiStateStyle{};
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

LayoutSize GuiWidget::measureContent(const GuiDrawContext& /*ctx*/,
                                     float /*max_width*/) const {
  return {};
}

void GuiWidget::arrangeChildren(GuiWidgetTree& tree, const Rect& available) {
  arrangeFlexChildren(tree, *this, available);
}

}  // namespace eng
