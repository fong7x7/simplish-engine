#include "flex-layout.h"

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

  bool sameRect(const Rect& a, const Rect& b) {
    return a.x == b.x && a.y == b.y && a.w == b.w && a.h == b.h;
  }

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

void GuiWidgetTree::computeLayout(const Rect& viewport,
                                  const GuiDrawContext& ctx) {
  if (root_id == GUI_WIDGET_ID_INVALID) {
    return;
  }
  measureWidget(root_id, ctx, viewport.w);
  arrangeWidget(root_id, viewport);
  startGlides();
  clearDirtyFlags();
}

void GuiWidgetTree::updateLayout(const Rect& viewport,
                                 const GuiDrawContext& ctx) {
  incremental_layout_ = true;
  computeLayout(viewport, ctx);
  incremental_layout_ = false;
}

void GuiWidgetTree::computeLayout(const Rect& viewport) {
  computeLayout(viewport, GuiDrawContext{});
}

void GuiWidgetTree::measureWidget(GuiWidgetId id, const GuiDrawContext& ctx) {
  measureWidget(id, ctx, -1.0f);
}

// NOLINTNEXTLINE(misc-no-recursion) -- tree traversal requires recursion
void GuiWidgetTree::measureWidget(GuiWidgetId id, const GuiDrawContext& ctx,
                                  float max_width) {
  auto* w = findWidget(id);
  // What is clean and measured against the same width still holds.
  if (w == nullptr || (incremental_layout_ && !w->tree_dirty &&
                       w->tree_measured_limit == max_width)) {
    return;
  }
  w->tree_measured_limit = max_width;
  const GuiDrawContext scoped = ctx.themedBy(w->subtree_theme.get());
  const float inner = contentWidthLimit(w->tree_layout, max_width);
  for (auto child : w->children) {
    if (const GuiWidget* c = findWidget(child)) {
      measureWidget(child, scoped, childWidthLimit(*c, inner));
    }
  }
  w->tree_measured = measureBorderBox(*this, *w, {scoped, max_width});
}

void GuiWidgetTree::arrangeWidget(GuiWidgetId id, const Rect& available) {
  auto* w = findWidget(id);
  // A clean subtree placed where it already is needs nothing done.
  if (w == nullptr ||
      (incremental_layout_ && !w->tree_dirty && sameRect(w->rect, available))) {
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
