#include <algorithm>
#include <cmath>
#include <engine/gui/gui-input.h>
#include <engine/gui/gui-widget-tree.h>
#include <engine/gui/layout-engine.h>
#include <limits>
#include <vector>

namespace eng {

/// Minimum focusable widget count for spatial navigation.
constexpr size_t MIN_NAV_WIDGETS = 2;
/// Half multiplier for computing center coordinate.
constexpr float HALF = 0.5f;

namespace {

  /// Check if point (px, py) is inside the rectangle.
  bool rectContains(const Rect& r, float px, float py) {
    return px >= r.x && px < r.x + r.w && py >= r.y && py < r.y + r.h;
  }

  // NOLINTNEXTLINE(misc-no-recursion) -- tree traversal requires recursion
  /// Parameters for recursive hit testing.
  struct HitTestParams {
    /// Widget tree to test against.
    const GuiWidgetTree& ctx;
    /// Cursor X position.
    float x;
    /// Cursor Y position.
    float y;
    /// Best hit result (updated in place).
    HitTestResult& best;
  };

  /// Sort child IDs by z_index ascending for correct hit-test ordering.
  std::vector<GuiWidgetId> sortedChildrenByZ(const GuiWidgetTree& ctx,
                                             const GuiWidget& parent) {
    auto ids = parent.children;
    std::ranges::stable_sort(ids, [&](GuiWidgetId a, GuiWidgetId b) {
      const auto* za = ctx.findWidget(a);
      const auto* zb = ctx.findWidget(b);
      int32_t va = (za != nullptr) ? za->z_index : 0;
      int32_t vb = (zb != nullptr) ? zb->z_index : 0;
      return va < vb;
    });
    return ids;
  }

  // NOLINTNEXTLINE(misc-no-recursion) -- tree traversal requires recursion
  void hitTestRecursive(const HitTestParams& p, GuiWidgetId id) {
    auto it = p.ctx.widget_nodes.find(id);
    if (it == p.ctx.widget_nodes.end()) {
      return;
    }
    const auto& w = *it->second;
    if (!w.visible || !rectContains(w.rect, p.x, p.y)) {
      return;
    }
    // Children are always on top of their parent in the tree.
    // Always update best — last matching node in pre-order wins.
    p.best.widget_id = w.widget_id;
    // Visit children in ascending z-order so topmost sibling wins last.
    for (auto child : sortedChildrenByZ(p.ctx, w)) {
      hitTestRecursive(p, child);
    }
  }

  // NOLINTNEXTLINE(misc-no-recursion) -- tree traversal requires recursion
  void collectFocusable(const GuiWidgetTree& ctx, GuiWidgetId id,
                        std::vector<GuiWidgetId>& out) {
    auto it = ctx.widget_nodes.find(id);
    if (it == ctx.widget_nodes.end()) {
      return;
    }
    if (it->second->tree_focusable && it->second->visible) {
      out.push_back(id);
    }
    for (auto child : it->second->children) {
      collectFocusable(ctx, child, out);
    }
  }

  float widgetCenterY(const GuiWidgetTree& ctx, GuiWidgetId id) {
    auto it = ctx.widget_nodes.find(id);
    if (it == ctx.widget_nodes.end()) {
      return 0.0f;
    }
    const auto& r = it->second->rect;
    return r.y + r.h * HALF;
  }

  /// Dispatch mouse event type to update context state.
  void dispatchMouseEvent(GuiWidgetTree& ctx, const GuiMouseEvent& event,
                          GuiWidgetId hit_id) {
    if (event.type == GuiMouseEventType::MOVE) {
      ctx.hovered_id = hit_id;
    } else if (event.type == GuiMouseEventType::BUTTON_DOWN) {
      ctx.pressed_id = hit_id;
    } else if (event.type == GuiMouseEventType::BUTTON_UP) {
      ctx.pressed_id = GUI_WIDGET_ID_INVALID;
    }
  }

  /// Get focusable list for the tree, empty if root invalid.
  std::vector<GuiWidgetId> gatherFocusable(const GuiWidgetTree& ctx) {
    std::vector<GuiWidgetId> result;
    if (ctx.root_id != GUI_WIDGET_ID_INVALID) {
      collectFocusable(ctx, ctx.root_id, result);
    }
    return result;
  }

  /// Cycle to next or previous focusable widget from current.
  GuiWidgetId cycleFocus(const std::vector<GuiWidgetId>& list,
                         GuiWidgetId current,
                         FocusTraversalDirection direction) {
    // NOLINTNEXTLINE(modernize-use-ranges,llvm-use-ranges) no ranges support
    auto it = std::find(list.begin(), list.end(), current);
    if (it == list.end()) {
      return list[0];
    }

    if (direction == FocusTraversalDirection::REVERSE) {
      return (it == list.begin()) ? list.back() : *(it - 1);
    }
    ++it;
    return (it == list.end()) ? list[0] : *it;
  }

  /// Mutable state for spatial nearest-widget search.
  struct SpatialSearch {
    /// Y-coordinate of the current widget center.
    float origin_y;
    /// Vertical search axis (Column = downward from current).
    FocusSpatialSearch axis;
    /// Best candidate found so far.
    GuiWidgetId best;
    /// Distance of best candidate.
    float best_dist;
  };

  /// Update best candidate if fid is closer in the search direction.
  void updateSpatialBest(const GuiWidgetTree& ctx, GuiWidgetId fid,
                         SpatialSearch& search) {
    float fy = widgetCenterY(ctx, fid);
    float dist = (search.axis == FocusSpatialSearch::DOWN)
                     ? (fy - search.origin_y)
                     : (search.origin_y - fy);
    if (dist > 0.0f && dist < search.best_dist) {
      search.best_dist = dist;
      search.best = fid;
    }
  }

  /// Find nearest focusable widget in the given spatial direction.
  GuiWidgetId findSpatialNearest(const GuiWidgetTree& ctx,
                                 const std::vector<GuiWidgetId>& list,
                                 GuiWidgetId current, FocusSpatialSearch axis) {
    SpatialSearch search{widgetCenterY(ctx, current), axis,
                         GUI_WIDGET_ID_INVALID,
                         std::numeric_limits<float>::max()};
    for (auto fid : list) {
      if (fid != current) {
        updateSpatialBest(ctx, fid, search);
      }
    }
    return search.best;
  }

}  // namespace

HitTestResult GuiWidgetTree::hitTest(float x, float y) const {
  HitTestResult result;
  if (root_id == GUI_WIDGET_ID_INVALID) {
    return result;
  }
  hitTestRecursive({*this, x, y, result}, root_id);
  return result;
}

bool GuiWidgetTree::routeMouseEvent(const GuiMouseEvent& event) {
  auto hit = hitTest(event.x, event.y);
  if (hit.widget_id == GUI_WIDGET_ID_INVALID) {
    return false;
  }
  dispatchMouseEvent(*this, event, hit.widget_id);
  return true;
}

bool GuiWidgetTree::routeKeyEvent(const GuiKeyEvent& event) {
  if (focused_id == GUI_WIDGET_ID_INVALID) {
    return false;
  }
  auto* w = findWidget(focused_id);
  if (w == nullptr) {
    return false;
  }
  (void)event;
  return true;
}

bool GuiWidgetTree::routeTextInput(std::string_view /*text*/) {
  if (focused_id == GUI_WIDGET_ID_INVALID) {
    return false;
  }
  return (findWidget(focused_id) != nullptr);
}

void GuiWidgetTree::setFocus(GuiWidgetId id) {
  auto* w = findWidget(id);
  if (w == nullptr || !w->tree_focusable) {
    return;
  }
  focused_id = id;
}

void GuiWidgetTree::advanceFocus(FocusTraversalDirection direction) {
  auto focusable = gatherFocusable(*this);
  if (focusable.empty()) {
    return;
  }
  focused_id = cycleFocus(focusable, focused_id, direction);
}

void GuiWidgetTree::navigateFocus(FlexDirection direction) {
  if (focused_id == GUI_WIDGET_ID_INVALID) {
    return;
  }
  auto focusable = gatherFocusable(*this);
  if (focusable.size() < MIN_NAV_WIDGETS) {
    return;
  }

  auto axis = (direction == FlexDirection::COLUMN) ? FocusSpatialSearch::DOWN
                                                   : FocusSpatialSearch::UP;
  auto best = findSpatialNearest(*this, focusable, focused_id, axis);
  if (best != GUI_WIDGET_ID_INVALID) {
    focused_id = best;
  }
}

}  // namespace eng
