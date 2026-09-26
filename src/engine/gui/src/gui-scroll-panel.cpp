#include "engine/gui/gui-scroll-panel.h"

#include "engine/gui/gui-draw-context.h"
#include "engine/gui/gui-widget-tree.h"
#include "flex-layout.h"

#include <algorithm>

namespace eng {

namespace {

  /// Thickness of the scroll thumb, and its gap from the edge.
  constexpr float THUMB_WIDTH = 4.0f;
  constexpr float THUMB_INSET = 2.0f;
  /// The shortest the thumb gets, so it stays something to see.
  constexpr float THUMB_MIN = 16.0f;

  /// Where a rect starts along an axis, and how long it is.
  struct Span {
    /// Its start along the axis.
    float start;
    /// Its length along it.
    float length;
  };

  /// @p rect along @p axis.
  Span spanOf(const Rect& rect, GuiScrollAxis axis) {
    return axis == GuiScrollAxis::VERTICAL ? Span{rect.y, rect.h}
                                           : Span{rect.x, rect.w};
  }

  /// The commands that step back and forward along @p axis.
  bool isAlong(GuiNavCommand command, GuiScrollAxis axis) {
    return axis == GuiScrollAxis::VERTICAL
               ? command == GuiNavCommand::UP || command == GuiNavCommand::DOWN
               : command == GuiNavCommand::LEFT ||
                     command == GuiNavCommand::RIGHT;
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
  return std::max(0.0f, content_ - viewport_);
}

void GuiScrollPanel::setScrollOffset(float offset) {
  offset_ = std::clamp(offset, 0.0f, maxScroll());
}

void GuiScrollPanel::arrangeChildren(GuiWidgetTree& tree,
                                     const Rect& available) {
  rect = available;
  tree_layout.direction = axis == GuiScrollAxis::VERTICAL
                              ? FlexDirection::COLUMN
                              : FlexDirection::ROW;
  sizeUnmeasured(tree);
  const LayoutSize flow = flowSize(tree, *this);
  content_ = axis == GuiScrollAxis::VERTICAL ? flow.h : flow.w;
  viewport_ = spanOf(viewport(), axis).length;
  setScrollOffset(offset_);
  // The children are laid out by flexbox, like any other.
  arrangeFlexChildren(tree, *this, scrolledBox());
}

DrawPos GuiScrollPanel::contentOrigin() const {
  const Rect box = scrolledBox();
  return {box.x, box.y};
}

Rect GuiScrollPanel::scrolledBox() const {
  const Edges& pad = tree_layout.padding;
  return axis == GuiScrollAxis::VERTICAL
             ? Rect{rect.x, rect.y - offset_, rect.w,
                    std::max(rect.h, content_ + pad.top + pad.bottom)}
             : Rect{rect.x - offset_, rect.y,
                    std::max(rect.w, content_ + pad.left + pad.right), rect.h};
}

void GuiScrollPanel::sizeUnmeasured(GuiWidgetTree& tree) const {
  for (const GuiWidgetId id : children) {
    GuiWidget* child = tree.findWidget(id);
    if (child == nullptr) {
      continue;
    }
    const bool vertical = axis == GuiScrollAxis::VERTICAL;
    const float own =
        vertical ? child->tree_layout.height : child->tree_layout.width;
    float& measured =
        vertical ? child->tree_measured.h : child->tree_measured.w;
    if (own < 0.0f && measured <= 0.0f) {
      measured = item_size;
    }
  }
}

bool GuiScrollPanel::scrollBy(float dx, float dy) {
  const float was = offset_;
  setScrollOffset(was + (axis == GuiScrollAxis::VERTICAL ? dy : dx));
  return offset_ != was;
}

bool GuiScrollPanel::handleScroll(const GuiScrollEvent& event) {
  (void)GuiPanel::handleScroll(event);
  // One wheel moves either way: a row scrolls sideways under it.
  const float step = -event.delta_y * wheel_step;
  return scrollBy(step, step);
}

bool GuiScrollPanel::revealChild(const Rect& child) {
  const Span view = spanOf(viewport(), axis);
  const Span item = spanOf(child, axis);
  const float before = view.start - item.start;
  const float after = item.start + item.length - (view.start + view.length);
  if (before > 0.0f) {
    return scrollBy(-before, -before);
  }
  // An item longer than the view shows its start rather than its end.
  const float forward = std::min(after, item.start - view.start);
  return after > 0.0f && scrollBy(forward, forward);
}

bool GuiScrollPanel::scrollByNav(GuiNavCommand command) {
  if (!isAlong(command, axis)) {
    return false;
  }
  const bool back =
      command == GuiNavCommand::UP || command == GuiNavCommand::LEFT;
  const float step = back ? -nav_step : nav_step;
  return scrollBy(step, step);
}

void GuiScrollPanel::arrangeAfterScroll(GuiWidgetTree& tree) {
  arrangeChildren(tree, rect);
}

std::optional<Rect> GuiScrollPanel::childClipRect() const {
  return viewport();
}

Rect GuiScrollPanel::thumbRect() const {
  const Span view = spanOf(viewport(), axis);
  const float length = std::max(THUMB_MIN, view.length * viewport_ / content_);
  const float at = view.start + std::max(0.0f, view.length - length) *
                                    (offset_ / maxScroll());
  return axis == GuiScrollAxis::VERTICAL
             ? Rect{rect.x + rect.w - THUMB_WIDTH - THUMB_INSET, at,
                    THUMB_WIDTH, length}
             : Rect{at, rect.y + rect.h - THUMB_WIDTH - THUMB_INSET, length,
                    THUMB_WIDTH};
}

void GuiScrollPanel::render(const GuiDrawContext& ctx) const {
  GuiPanel::render(ctx);
  if (maxScroll() <= 0.0f || content_ <= 0.0f) {
    return;
  }
  ctx.drawRoundedRect(
      thumbRect(),
      GuiColor::applyOpacity(ctx.activeTheme().palette.text_muted, opacity),
      THUMB_WIDTH * 0.5f);
}

}  // namespace eng
