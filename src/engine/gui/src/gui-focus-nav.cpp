// Focus navigation on GuiWidgetTree: moving focus between widgets without a
// pointer — spatially for the d-pad and arrows, in order for Tab and the
// shoulders — offering each command to the focused widget first, keeping
// it inside a menu's scope, scrolling it into view, and ringing where
// focus is. Tree nodes and overlay components both take part. Kept apart
// from gui-widget-tree.cpp because none of it touches the pointer path.

#include <algorithm>
#include <cmath>
#include <engine/gui/gui-renderer.h>
#include <engine/gui/gui-widget-tree.h>
#include <iterator>
#include <limits>
#include <optional>
#include <vector>

namespace eng {

namespace {

  /// A direction's unit vector on the screen, +Y down.
  struct NavAxis {
    /// X of the unit vector.
    float x;
    /// Y of the unit vector.
    float y;
  };

  /// How much being off the line costs against being far along it: twice,
  /// so a grid moves by its rows and columns rather than diagonally.
  constexpr float OFF_AXIS_WEIGHT = 2.0f;
  /// How far along the direction a candidate must be to count as "that
  /// way", in logical pixels — so a widget level with focus is not above it.
  constexpr float MIN_ALONG = 0.5f;
  /// How far the ring sits outside the widget; the theme sets its width.
  constexpr float RING_OUTSET = 3.0f;
  constexpr float RING_RADIUS = 4.0f;
  /// Half, for a centre.
  constexpr float HALF = 0.5f;

  /// The unit vector @p command points along, or nothing for a command
  /// that is not a direction.
  std::optional<NavAxis> axisOf(GuiNavCommand command) {
    switch (command) {
      case GuiNavCommand::UP:
        return NavAxis{0.0f, -1.0f};
      case GuiNavCommand::DOWN:
        return NavAxis{0.0f, 1.0f};
      case GuiNavCommand::LEFT:
        return NavAxis{-1.0f, 0.0f};
      case GuiNavCommand::RIGHT:
        return NavAxis{1.0f, 0.0f};
      default:
        return std::nullopt;
    }
  }

  /// Whether @p widget can hold navigation focus.
  bool canFocus(const GuiWidget& widget) {
    return widget.tree_focusable && widget.visible && !widget.disabled;
  }

  /// Every visible focusable widget under @p id, pre-order, into @p out. An
  /// invisible widget hides its whole subtree.
  // NOLINTNEXTLINE(misc-no-recursion) -- tree traversal requires recursion
  void collectFocusable(GuiWidgetTree& tree, GuiWidgetId id,
                        std::vector<GuiWidget*>& out) {
    GuiWidget* widget = tree.findWidget(id);
    if (widget == nullptr || !widget->visible) {
      return;
    }
    if (canFocus(*widget)) {
      out.push_back(widget);
    }
    for (const GuiWidgetId child : widget->children) {
      collectFocusable(tree, child, out);
    }
  }

  /// The centre of @p rect.
  NavAxis centreOf(const Rect& rect) {
    return {rect.x + rect.w * HALF, rect.y + rect.h * HALF};
  }

  /// How far @p to is from @p from in the direction @p axis — along it,
  /// plus the weighted distance off it — or nothing when @p to does not
  /// lie that way.
  std::optional<float> navScore(const Rect& from, const Rect& to,
                                NavAxis axis) {
    const NavAxis a = centreOf(from);
    const NavAxis b = centreOf(to);
    const float dx = b.x - a.x;
    const float dy = b.y - a.y;
    const float along = dx * axis.x + dy * axis.y;
    if (along < MIN_ALONG) {
      return std::nullopt;
    }
    const float off = std::abs(dx * axis.y - dy * axis.x);
    return along + off * OFF_AXIS_WEIGHT;
  }

  /// The best of @p candidates to move to from @p current along @p axis,
  /// or null. Ties go to the earlier in navigation order, so the answer
  /// never depends on anything but the layout.
  GuiWidget* bestInDirection(const std::vector<GuiWidget*>& candidates,
                             const GuiWidget& current, NavAxis axis) {
    GuiWidget* best = nullptr;
    float best_score = std::numeric_limits<float>::max();
    for (GuiWidget* widget : candidates) {
      if (widget == &current) {
        continue;
      }
      const std::optional<float> score =
          navScore(current.rect, widget->rect, axis);
      if (score && *score < best_score) {
        best_score = *score;
        best = widget;
      }
    }
    return best;
  }

  /// Whether @p id is @p root or lies under it.
  bool isWithin(const GuiWidgetTree& tree, GuiWidgetId id, GuiWidgetId root) {
    for (GuiWidgetId at = id; at != GUI_WIDGET_ID_INVALID;) {
      if (at == root) {
        return true;
      }
      const GuiWidget* widget = tree.findWidget(at);
      at = widget != nullptr ? widget->parent_id : GUI_WIDGET_ID_INVALID;
    }
    return false;
  }

  /// Whether @p widget is a text field, which CONFIRM starts typing in.
  bool isTextField(const GuiWidget& widget) {
    return widget.widget_type == GuiWidgetType::TEXT_INPUT ||
           widget.widget_type == GuiWidgetType::TEXT_AREA;
  }

  /// The one entry in focus order after or before @p current in @p list,
  /// wrapping; the first when @p current is not in it.
  GuiWidget* cycleFocus(const std::vector<GuiWidget*>& list,
                        const GuiWidget* current,
                        FocusTraversalDirection direction) {
    const auto it = std::ranges::find(list, current);
    if (it == list.end()) {
      return list.front();
    }
    if (direction == FocusTraversalDirection::REVERSE) {
      return it == list.begin() ? list.back() : *(it - 1);
    }
    return it + 1 == list.end() ? list.front() : *(it + 1);
  }

  /// Clips drawing to a rect for as long as it lives, when there is one
  /// and something to draw with.
  class ScopedClip {
  public:
    /// Push @p clip onto @p ctx's scissor stack, if both exist.
    ScopedClip(const GuiDrawContext& ctx, const std::optional<Rect>& clip) {
      if (clip && ctx.renderer != nullptr) {
        renderer_ = ctx.renderer;
        renderer_->pushScissor(*clip);
      }
    }
    ~ScopedClip() {
      if (renderer_ != nullptr) {
        renderer_->popScissor();
      }
    }
    ScopedClip(const ScopedClip&) = delete;
    ScopedClip& operator=(const ScopedClip&) = delete;
    ScopedClip(ScopedClip&&) = delete;
    ScopedClip& operator=(ScopedClip&&) = delete;

  private:
    /// The renderer clipped, or null when nothing was pushed.
    GuiRendererContext* renderer_ = nullptr;
  };

  /// Draws a widget and its subtree moved and faded as one: sets the
  /// renderer's transform to the widget's `render_scale` and
  /// `render_offset` inside its parent's for the widget and its children,
  /// and, for the children, multiplies the alpha by the widget's opacity
  /// (the widget applies its own to itself). Restores both on the way out.
  class ScopedLayer {
  public:
    ScopedLayer(const GuiDrawContext& ctx, const GuiWidget& widget)
      : renderer_(ctx.renderer), widget_(widget) {
      if (renderer_ == nullptr) {
        return;
      }
      transform_ = renderer_->transform;
      alpha_ = renderer_->alpha_scale;
      if (widget.render_scale != 1.0f || widget.render_offset_x != 0.0f ||
          widget.render_offset_y != 0.0f) {
        renderer_->transform = transform_.after(GuiRenderTransform::about(
            widget.rect, widget.render_scale, widget.render_offset_x,
            widget.render_offset_y));
      }
    }
    ~ScopedLayer() {
      if (renderer_ != nullptr) {
        renderer_->transform = transform_;
        renderer_->alpha_scale = alpha_;
      }
    }
    ScopedLayer(const ScopedLayer&) = delete;
    ScopedLayer& operator=(const ScopedLayer&) = delete;
    ScopedLayer(ScopedLayer&&) = delete;
    ScopedLayer& operator=(ScopedLayer&&) = delete;

    /// Fade what is drawn from here on — the children — by the widget's
    /// opacity.
    void enterChildren() const {
      if (renderer_ != nullptr) {
        renderer_->alpha_scale = alpha_ * widget_.opacity;
      }
    }

  private:
    /// The renderer drawn to, or null when there is none.
    GuiRendererContext* renderer_ = nullptr;
    /// The widget whose layer this is.
    const GuiWidget& widget_;
    /// The transform to restore.
    GuiRenderTransform transform_{};
    /// The alpha scale to restore.
    float alpha_ = 1.0f;
  };

  /// @p rect grown by the ring's outset on every side.
  Rect ringAround(const Rect& rect) {
    return {rect.x - RING_OUTSET, rect.y - RING_OUTSET,
            rect.w + RING_OUTSET * 2.0f, rect.h + RING_OUTSET * 2.0f};
  }

}  // namespace

GuiWidgetId GuiWidgetTree::navRoot() const {
  return findWidget(focus_scope_id) != nullptr ? focus_scope_id : root_id;
}

bool GuiWidgetTree::isTreeNode(const GuiWidget& widget) const {
  return findWidget(widget.widget_id) == &widget;
}

GuiWidget* GuiWidgetTree::focusedWidget() {
  return findWidget(focused_id);
}

const GuiWidget* GuiWidgetTree::focusedWidget() const {
  return findWidget(focused_id);
}

std::vector<GuiWidget*> GuiWidgetTree::focusableInScope() {
  std::vector<GuiWidget*> out;
  if (navRoot() != GUI_WIDGET_ID_INVALID) {
    collectFocusable(*this, navRoot(), out);
  }
  return out;
}

bool GuiWidgetTree::hasNavFocus() {
  const GuiWidget* focused = focusedWidget();
  const std::vector<GuiWidget*> focusable = focusableInScope();
  return focused != nullptr &&
         std::ranges::find(focusable, focused) != focusable.end();
}

void GuiWidgetTree::moveFocus(GuiWidget* widget) {
  if (focused_input_ != nullptr && focused_input_ != widget) {
    clearFocus();
  }
  GuiWidget* was = focusedWidget();
  focused_id = widget != nullptr ? widget->widget_id : GUI_WIDGET_ID_INVALID;
  revealFocus();
  if (was != widget) {
    announceFocusChange(was, widget);
  }
}

void GuiWidgetTree::announceFocusChange(GuiWidget* was, GuiWidget* now) {
  if (was != nullptr) {
    was->handleFocusChange(GuiFocusChange::LOST);
  }
  if (now != nullptr) {
    now->handleFocusChange(GuiFocusChange::GAINED);
  }
}

void GuiWidgetTree::setFocus(GuiWidgetId id) {
  GuiWidget* widget = findWidget(id);
  if (widget != nullptr && widget->tree_focusable) {
    moveFocus(widget);
  }
}

void GuiWidgetTree::setFocus(GuiWidget& widget) {
  if (widget.tree_focusable && isTreeNode(widget)) {
    moveFocus(&widget);
  }
}

void GuiWidgetTree::setFocusScope(GuiWidgetId scope) {
  focus_scope_id = scope;
  if (scope == GUI_WIDGET_ID_INVALID || isWithin(*this, focused_id, scope)) {
    return;
  }
  const std::vector<GuiWidget*> focusable = focusableInScope();
  moveFocus(focusable.empty() ? nullptr : focusable.front());
}

void GuiWidgetTree::advanceFocus(FocusTraversalDirection direction) {
  const std::vector<GuiWidget*> focusable = focusableInScope();
  if (!focusable.empty()) {
    moveFocus(cycleFocus(focusable, focusedWidget(), direction));
  }
}

bool GuiWidgetTree::navigateFocus(GuiNavCommand command) {
  const std::optional<NavAxis> axis = axisOf(command);
  const GuiWidget* current = focusedWidget();
  if (!axis || current == nullptr) {
    return false;
  }
  GuiWidget* best = bestInDirection(focusableInScope(), *current, *axis);
  if (best == nullptr) {
    return false;
  }
  moveFocus(best);
  return true;
}

bool GuiWidgetTree::bubbleNav(GuiNavCommand command) {
  const GuiWidgetId stop = navRoot();
  for (GuiWidgetId at = focused_id; at != GUI_WIDGET_ID_INVALID;) {
    GuiWidget* widget = findWidget(at);
    if (widget == nullptr) {
      return false;
    }
    if (widget->handleNav(command)) {
      return true;
    }
    at = at == stop ? GUI_WIDGET_ID_INVALID : widget->parent_id;
  }
  return false;
}

bool GuiWidgetTree::routeTextNav(GuiNavCommand command) {
  if (command == GuiNavCommand::CANCEL && focused_input_ != nullptr) {
    clearFocus();
    return true;
  }
  GuiWidget* widget = focusedWidget();
  if (command != GuiNavCommand::CONFIRM || widget == nullptr ||
      !isTextField(*widget)) {
    return false;
  }
  // Both field types derive from GuiTextInput and set their own tag in
  // their constructors, so the tag is what makes the cast safe; RTTI is off.
  // NOLINTNEXTLINE(cppcoreguidelines-pro-type-static-cast-downcast)
  applyTextInputFocus(static_cast<GuiTextInput*>(widget));
  return true;
}

bool GuiWidgetTree::focusFirst(GuiNavCommand command) {
  const std::vector<GuiWidget*> focusable = focusableInScope();
  if (command == GuiNavCommand::CANCEL || focusable.empty()) {
    return false;
  }
  moveFocus(focusable.front());
  return true;
}

bool GuiWidgetTree::moveNavFocus(GuiNavCommand command) {
  if (command == GuiNavCommand::NEXT || command == GuiNavCommand::PREVIOUS) {
    advanceFocus(command == GuiNavCommand::NEXT
                     ? FocusTraversalDirection::FORWARD
                     : FocusTraversalDirection::REVERSE);
    return true;
  }
  return navigateFocus(command);
}

bool GuiWidgetTree::scrollNav(GuiNavCommand command) {
  const GuiWidget* focused = findWidget(focused_id);
  GuiWidget* at = focused != nullptr ? findWidget(focused->parent_id) : nullptr;
  for (; at != nullptr; at = findWidget(at->parent_id)) {
    if (at->scrollByNav(command)) {
      afterScroll(*at);
      return true;
    }
  }
  return false;
}

bool GuiWidgetTree::scrollFocusBy(float dx, float dy) {
  const GuiWidget* focused = findWidget(focused_id);
  GuiWidget* at = focused != nullptr ? findWidget(focused->parent_id) : nullptr;
  for (; at != nullptr; at = findWidget(at->parent_id)) {
    if (at->scrollBy(dx, dy)) {
      afterScroll(*at);
      return true;
    }
  }
  return false;
}

bool GuiWidgetTree::routeNav(GuiNavCommand command) {
  focus_visibility = GuiFocusVisibility::SHOWN;
  if (!hasNavFocus()) {
    return focusFirst(command);
  }
  return routeTextNav(command) || bubbleNav(command) || moveNavFocus(command) ||
         scrollNav(command);
}

void GuiWidgetTree::revealFocus() {
  const GuiWidget* focused = findWidget(focused_id);
  GuiWidget* at = focused != nullptr ? findWidget(focused->parent_id) : nullptr;
  for (; at != nullptr; at = findWidget(at->parent_id)) {
    // Re-read each time: an inner scroll has just moved the focused rect.
    if (at->revealChild(focused->rect)) {
      afterScroll(*at);
    }
  }
}

void GuiWidgetTree::afterScroll(GuiWidget& widget) {
  widget.arrangeAfterScroll(*this);
}

std::optional<Rect> GuiWidgetTree::ancestorClip(const GuiWidget& widget) const {
  std::optional<Rect> clip;
  const GuiWidget* at =
      isTreeNode(widget) ? findWidget(widget.parent_id) : nullptr;
  for (; at != nullptr; at = findWidget(at->parent_id)) {
    if (const std::optional<Rect> own = at->childClipRect()) {
      clip = clip ? intersectRects(*clip, *own) : *own;
    }
  }
  return clip;
}

void GuiWidgetTree::renderFocusRing(const GuiDrawContext& ctx) const {
  const GuiWidget* widget = focusedWidget();
  if (focus_visibility != GuiFocusVisibility::SHOWN || widget == nullptr ||
      !canFocus(*widget)) {
    return;
  }
  const ScopedClip clip{ctx, ancestorClip(*widget)};
  const GuiTheme* own = themeAt(widget->widget_id);
  const GuiTheme& theme = own != nullptr ? *own : ctx.activeTheme();
  ctx.drawRoundedBorderRect({ringAround(widget->rect), theme.palette.focus_ring,
                             RING_RADIUS, theme.focus_ring_width});
}

// NOLINTNEXTLINE(misc-no-recursion) -- tree traversal requires recursion
void GuiWidgetTree::renderTreeNode(GuiWidgetId id,
                                   const GuiDrawContext& ctx) const {
  const GuiWidget* widget = findWidget(id);
  if (widget == nullptr || !widget->visible) {
    return;
  }
  const GuiDrawContext scoped = ctx.themedBy(widget->subtree_theme.get());
  const ScopedLayer layer{scoped, *widget};
  widget->render(scoped);
  layer.enterChildren();
  const ScopedClip clip{scoped, widget->childClipRect()};
  for (const GuiWidgetId child : sortedChildIdsByZ(*this, *widget)) {
    renderTreeNode(child, scoped);
  }
}

}  // namespace eng
