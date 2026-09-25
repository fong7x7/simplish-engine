#include <algorithm>
#include <cmath>
#include <engine/gui/gui-input.h>
#include <engine/gui/gui-widget-tree.h>
#include <engine/gui/layout-engine.h>
#include <vector>

namespace eng {

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
    // Always update best — last matching node in pre-order wins — unless
    // the pointer passes through this one to what is under it.
    if (!w.pointer_through) {
      p.best.widget_id = w.widget_id;
    }
    // Visit children in ascending z-order so topmost sibling wins last.
    for (auto child : sortedChildrenByZ(p.ctx, w)) {
      hitTestRecursive(p, child);
    }
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

}  // namespace eng
