// Focus navigation on GuiWidgetTree: moving focus between widgets without a
// pointer — spatially for the d-pad, in order for Tab and the shoulders —
// offering each command to the focused widget first, keeping it inside a
// menu's scope, and ringing where focus is. Kept apart from
// gui-widget-tree.cpp because none of it touches the pointer path.

#include <algorithm>
#include <cmath>
#include <engine/gui/gui-widget-tree.h>
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
  /// How far the ring sits outside the widget, and how thick it is.
  constexpr float RING_OUTSET = 3.0f;
  constexpr float RING_WIDTH = 2.0f;
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
    return widget.tree_focusable && widget.visible;
  }

  /// Every visible focusable widget under @p id, pre-order, into @p out. An
  /// invisible widget hides its whole subtree.
  // NOLINTNEXTLINE(misc-no-recursion) -- tree traversal requires recursion
  void collectFocusable(const GuiWidgetTree& tree, GuiWidgetId id,
                        std::vector<GuiWidgetId>& out) {
    const GuiWidget* widget = tree.findWidget(id);
    if (widget == nullptr || !widget->visible) {
      return;
    }
    if (widget->tree_focusable) {
      out.push_back(id);
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
  /// or GUI_WIDGET_ID_INVALID. Ties go to the earlier in tree order, so the
  /// answer never depends on anything but the layout.
  GuiWidgetId bestInDirection(const GuiWidgetTree& tree,
                              const std::vector<GuiWidgetId>& candidates,
                              const GuiWidget& current, NavAxis axis) {
    GuiWidgetId best = GUI_WIDGET_ID_INVALID;
    float best_score = std::numeric_limits<float>::max();
    for (const GuiWidgetId id : candidates) {
      const GuiWidget* widget = tree.findWidget(id);
      if (id == current.widget_id || widget == nullptr) {
        continue;
      }
      const std::optional<float> score =
          navScore(current.rect, widget->rect, axis);
      if (score && *score < best_score) {
        best_score = *score;
        best = id;
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
  GuiWidgetId cycleFocus(const std::vector<GuiWidgetId>& list,
                         GuiWidgetId current,
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

}  // namespace

GuiWidgetId GuiWidgetTree::navRoot() const {
  return findWidget(focus_scope_id) != nullptr ? focus_scope_id : root_id;
}

std::vector<GuiWidgetId> GuiWidgetTree::focusableInScope() const {
  std::vector<GuiWidgetId> out;
  if (navRoot() != GUI_WIDGET_ID_INVALID) {
    collectFocusable(*this, navRoot(), out);
  }
  return out;
}

bool GuiWidgetTree::hasNavFocus() const {
  const std::vector<GuiWidgetId> focusable = focusableInScope();
  return std::ranges::find(focusable, focused_id) != focusable.end();
}

void GuiWidgetTree::moveFocus(GuiWidgetId id) {
  if (focused_input_ != nullptr && focused_input_->widget_id != id) {
    clearFocus();
  }
  focused_id = id;
}

void GuiWidgetTree::setFocus(GuiWidgetId id) {
  const GuiWidget* widget = findWidget(id);
  if (widget != nullptr && widget->tree_focusable) {
    moveFocus(id);
  }
}

void GuiWidgetTree::setFocusScope(GuiWidgetId scope) {
  focus_scope_id = scope;
  if (scope == GUI_WIDGET_ID_INVALID || isWithin(*this, focused_id, scope)) {
    return;
  }
  const std::vector<GuiWidgetId> focusable = focusableInScope();
  moveFocus(focusable.empty() ? GUI_WIDGET_ID_INVALID : focusable.front());
}

void GuiWidgetTree::advanceFocus(FocusTraversalDirection direction) {
  const std::vector<GuiWidgetId> focusable = focusableInScope();
  if (!focusable.empty()) {
    moveFocus(cycleFocus(focusable, focused_id, direction));
  }
}

bool GuiWidgetTree::navigateFocus(GuiNavCommand command) {
  const std::optional<NavAxis> axis = axisOf(command);
  const GuiWidget* current = findWidget(focused_id);
  if (!axis || current == nullptr) {
    return false;
  }
  const GuiWidgetId best =
      bestInDirection(*this, focusableInScope(), *current, *axis);
  if (best == GUI_WIDGET_ID_INVALID) {
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
  GuiWidget* widget = findWidget(focused_id);
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
  const std::vector<GuiWidgetId> focusable = focusableInScope();
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

bool GuiWidgetTree::routeNav(GuiNavCommand command) {
  focus_visibility = GuiFocusVisibility::SHOWN;
  if (!hasNavFocus()) {
    return focusFirst(command);
  }
  return routeTextNav(command) || bubbleNav(command) || moveNavFocus(command);
}

void GuiWidgetTree::renderFocusRing(const GuiDrawContext& ctx) const {
  const GuiWidget* widget = findWidget(focused_id);
  if (focus_visibility != GuiFocusVisibility::SHOWN || widget == nullptr ||
      !canFocus(*widget)) {
    return;
  }
  const Rect& at = widget->rect;
  const Rect ring{at.x - RING_OUTSET, at.y - RING_OUTSET,
                  at.w + RING_OUTSET * 2.0f, at.h + RING_OUTSET * 2.0f};
  const GuiStyle& style =
      active_style_ != nullptr ? *active_style_ : GuiStyle::dark();
  ctx.drawRoundedBorderRect({ring, style.focus_ring, RING_RADIUS, RING_WIDTH});
}

}  // namespace eng
