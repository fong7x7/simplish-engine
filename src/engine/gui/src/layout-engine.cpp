#include <algorithm>
#include <cmath>
#include <engine/gui/gui-rect.h>
#include <engine/gui/gui-widget-tree.h>
#include <engine/gui/layout-engine.h>

namespace eng {

/// Friction coefficient for inertial scroll decay per second.
constexpr float SCROLL_FRICTION = 5.0f;
/// Minimum velocity threshold below which scrolling stops.
constexpr float SCROLL_VELOCITY_EPSILON = 0.01f;

namespace {

  /// Clamp a value to [min_val, max_val].
  float clampf(float val, float min_val, float max_val) {
    return std::max(min_val, std::min(val, max_val));
  }

  /// Clamp scroll offsets to content bounds.
  void clampScrollOffsets(ScrollState& scroll) {
    scroll.offset_x = clampf(scroll.offset_x, 0.0f, scroll.content_w);
    scroll.offset_y = clampf(scroll.offset_y, 0.0f, scroll.content_h);
  }

  /// Apply velocity to offset and decay velocity.
  void applyScrollVelocity(ScrollState& scroll, float dt) {
    scroll.offset_x += scroll.velocity_x * dt;
    scroll.offset_y += scroll.velocity_y * dt;
    float decay = std::exp(-SCROLL_FRICTION * dt);
    scroll.velocity_x *= decay;
    scroll.velocity_y *= decay;
  }

}  // namespace

void GuiWidgetTree::computeLayout(const Rect& viewport) {
  if (root_id == GUI_WIDGET_ID_INVALID) {
    return;
  }
  measureWidget(root_id);
  arrangeWidget(root_id, viewport);
  clearDirtyFlags();
}

// NOLINTNEXTLINE(misc-no-recursion) -- tree traversal requires recursion
void GuiWidgetTree::measureWidget(GuiWidgetId id) {
  auto* w = findWidget(id);
  if (w == nullptr) {
    return;
  }
  for (auto child : w->children) {
    measureWidget(child);
  }
}

void GuiWidgetTree::arrangeWidget(GuiWidgetId id, const Rect& available) {
  auto* w = findWidget(id);
  if (w == nullptr) {
    return;
  }
  w->rect = available;
  w->arrangeChildren(*this, available);
}

void updateScroll(ScrollState& scroll, float dt) {
  if (std::abs(scroll.velocity_x) < SCROLL_VELOCITY_EPSILON &&
      std::abs(scroll.velocity_y) < SCROLL_VELOCITY_EPSILON) {
    return;
  }
  applyScrollVelocity(scroll, dt);
  clampScrollOffsets(scroll);
}

void scrollBy(ScrollState& scroll, float dx, float dy) {
  scroll.offset_x += dx;
  scroll.offset_y += dy;
  clampScrollOffsets(scroll);
}

}  // namespace eng
