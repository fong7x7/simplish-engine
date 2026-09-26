/// @file gui-layout-overlay.cpp
/// @brief The layout overlay: every box as a hairline, and the hovered
/// widget's box model, tinted as a browser's inspector tints it.

#include <array>
#include <cstdio>
#include <engine/gui/gui-draw-context.h>
#include <engine/gui/gui-widget-tree.h>
#include <string>

namespace eng {

namespace {

  /// The inspector's own colours — the ones every browser's devtools use,
  /// so they read at a glance — not the theme's: the overlay describes the
  /// theme's work, and must stand out from it.
  constexpr GuiColor OUTLINE{80, 200, 255, 90};
  /// A margin's tint.
  constexpr GuiColor MARGIN{249, 204, 157, 140};
  /// A border box's edge.
  constexpr GuiColor BORDER{255, 229, 153, 220};
  /// Padding's tint.
  constexpr GuiColor PADDING{195, 208, 139, 140};
  /// The content box's tint.
  constexpr GuiColor CONTENT{140, 182, 192, 140};
  /// The name tag's box and text.
  constexpr GuiColor TAG_FILL{24, 24, 28, 235};
  /// The name tag's text.
  constexpr GuiColor TAG_TEXT{240, 240, 244, 255};
  /// Space inside the name tag, and between it and the box.
  constexpr float TAG_PAD = 4.0f;

  /// @p box grown by @p e on each side (shrunk, for negative edges).
  Rect grown(const Rect& box, const Edges& e) {
    return {box.x - e.left, box.y - e.top, box.w + e.left + e.right,
            box.h + e.top + e.bottom};
  }

  /// @p e with every side negated.
  Edges negated(const Edges& e) {
    return {-e.top, -e.right, -e.bottom, -e.left};
  }

  /// Fill the ring between @p outer and @p inner — the part of @p outer
  /// the edges @p e of it cover — with @p color.
  void fillRing(const GuiDrawContext& ctx, const Rect& outer, const Edges& e,
                GuiColor color) {
    const float mid_h = outer.h - e.top - e.bottom;
    ctx.drawFilledRect({outer.x, outer.y, outer.w, e.top}, color);
    ctx.drawFilledRect(
        {outer.x, outer.y + outer.h - e.bottom, outer.w, e.bottom}, color);
    ctx.drawFilledRect({outer.x, outer.y + e.top, e.left, mid_h}, color);
    ctx.drawFilledRect(
        {outer.x + outer.w - e.right, outer.y + e.top, e.right, mid_h}, color);
  }

  /// What the tag over @p widget says: its name and its size.
  std::string tagOf(const GuiWidget& widget) {
    const std::string& name =
        !widget.id.empty() ? widget.id : widget.debug_name;
    std::array<char, 48> size{};
    (void)std::snprintf(size.data(), size.size(), "%.0f × %.0f",
                        static_cast<double>(widget.rect.w),
                        static_cast<double>(widget.rect.h));
    return (name.empty() ? std::string("widget") : name) + "  " + size.data();
  }

  /// Draw @p widget's name tag just above @p margin_box, or inside its top
  /// when there is no room above.
  void drawTag(const GuiDrawContext& ctx, const GuiWidget& widget,
               const Rect& margin_box) {
    const std::string tag = tagOf(widget);
    const GuiFont& font = ctx.activeTheme().font(GuiTextRole::CAPTION);
    const float h = ctx.fontMetrics(font).line_height + TAG_PAD * 2.0f;
    const float w = ctx.measureText(tag, font) + TAG_PAD * 4.0f;
    const float y = margin_box.y - h >= 0.0f ? margin_box.y - h : margin_box.y;
    ctx.drawRoundedRect({margin_box.x, y, w, h}, TAG_FILL, TAG_PAD);
    ctx.drawText({.text = tag,
                  .pos = {margin_box.x + TAG_PAD * 2.0f, y + TAG_PAD},
                  .color = TAG_TEXT,
                  .font = font});
  }

  /// Draw @p widget's box model: margin, border edge, padding, content.
  void drawBoxModel(const GuiDrawContext& ctx, const GuiWidget& widget) {
    const LayoutStyle& style = widget.tree_layout;
    const Rect margin_box = grown(widget.rect, style.margin);
    fillRing(ctx, margin_box, style.margin, MARGIN);
    fillRing(ctx, widget.rect, style.padding, PADDING);
    ctx.drawFilledRect(grown(widget.rect, negated(style.padding)), CONTENT);
    ctx.drawRect({.rect = widget.rect,
                  .border = {1.0f, 1.0f, 1.0f, 1.0f},
                  .border_color = BORDER});
    drawTag(ctx, widget, margin_box);
  }

  /// How many ancestors @p id has in @p tree.
  int depthOf(const GuiWidgetTree& tree, GuiWidgetId id) {
    int depth = 0;
    for (const GuiWidget* w = tree.findWidget(id); w != nullptr;
         w = tree.findWidget(w->parent_id)) {
      ++depth;
    }
    return depth;
  }

}  // namespace

const GuiWidget* GuiWidgetTree::inspectedWidget() const {
  const GuiWidget* deepest = nullptr;
  int deepest_depth = -1;
  for (const auto& entry : widget_nodes) {
    const GuiWidget& w = *entry.second;
    const int depth = w.hovered && w.visible ? depthOf(*this, entry.first) : -1;
    if (depth > deepest_depth) {
      deepest = &w;
      deepest_depth = depth;
    }
  }
  return deepest;
}

void GuiWidgetTree::renderLayoutOverlay(const GuiDrawContext& ctx) const {
  if (layout_overlay == GuiLayoutOverlay::OFF) {
    return;
  }
  visitDrawOrder([&ctx](const GuiWidget& widget) {
    if (widget.visible && widget.rect.w > 0.0f && widget.rect.h > 0.0f) {
      ctx.drawRect({.rect = widget.rect,
                    .border = {1.0f, 1.0f, 1.0f, 1.0f},
                    .border_color = OUTLINE});
    }
  });
  if (const GuiWidget* inspected = inspectedWidget()) {
    drawBoxModel(ctx, *inspected);
  }
}

}  // namespace eng
