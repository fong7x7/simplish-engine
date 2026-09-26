/// @file gui-widget-tree-overlay.cpp
/// @brief The tree's overlay layer, its layout flag, and tooltips.

#include <engine/gui/gui-panel.h>
#include <engine/gui/gui-popover-placement.h>
#include <engine/gui/gui-widget-tree.h>

namespace eng {

namespace {

  /// Padding round a tooltip's text, up and down and across.
  constexpr float TIP_PAD_Y = 5.0f;
  constexpr float TIP_PAD_X = 8.0f;

  /// Make @p layer the see-through panel over the whole root.
  void styleOverlayLayer(GuiPanel& layer) {
    layer.fill_color = GuiColor{0, 0, 0, 0};
    layer.pointer_through = true;
    layer.z_index = GUI_OVERLAY_LAYER_Z;
    layer.debug_name = "gui-overlay-layer";
    layer.tree_layout.position = PositionMode::ABSOLUTE;
    layer.tree_layout.abs_right = 0.0f;
    layer.tree_layout.abs_bottom = 0.0f;
  }

  /// Draw @p target's tooltip under it, kept inside @p viewport.
  void drawTooltip(const GuiDrawContext& ctx, const GuiWidget& target,
                   const Rect& viewport) {
    const GuiTheme& theme = ctx.activeTheme();
    const GuiFont& font = theme.font(GuiTextRole::CAPTION);
    const LayoutSize size{ctx.measureText(target.tooltip, font) + TIP_PAD_X * 2,
                          ctx.fontMetrics(font).line_height + TIP_PAD_Y * 2};
    const Rect box =
        placePopover(target.rect, size, viewport,
                     {.side = GuiPopoverSide::BELOW, .align = Align::CENTER});
    ctx.drawBox(box, theme.menu, 1.0f);
    ctx.drawText({.text = target.tooltip,
                  .pos = {box.x + TIP_PAD_X, box.y + TIP_PAD_Y},
                  .color = theme.palette.text,
                  .font = font});
  }

}  // namespace

GuiWidgetId GuiWidgetTree::overlayLayer() {
  if (findWidget(overlay_layer_) != nullptr) {
    return overlay_layer_;
  }
  if (root_id == GUI_WIDGET_ID_INVALID) {
    return GUI_WIDGET_ID_INVALID;
  }
  overlay_layer_ = createWidget(GuiWidgetType::PANEL, root_id);
  auto* layer = dynamic_cast<GuiPanel*>(findWidget(overlay_layer_));
  styleOverlayLayer(*layer);
  // Covers the root now, not only after the next layout.
  layer->rect = findWidget(root_id)->rect;
  return overlay_layer_;
}

bool GuiWidgetTree::needsLayout() const {
  const GuiWidget* root = findWidget(root_id);
  return root != nullptr && root->tree_dirty;
}

void GuiWidgetTree::trackTooltip(GuiWidgetId hovered) {
  const GuiWidget* w = findWidget(hovered);
  while (w != nullptr && w->tooltip.empty()) {
    w = findWidget(w->parent_id);
  }
  const GuiWidgetId target =
      w != nullptr ? w->widget_id : GUI_WIDGET_ID_INVALID;
  if (target != tooltip_target_) {
    tooltip_target_ = target;
    tooltip_seconds_ = 0.0f;
    tooltip_dismissed_ = false;
  }
}

void GuiWidgetTree::renderTooltip(const GuiDrawContext& ctx) const {
  const GuiWidget* target = findWidget(tooltip_target_);
  const GuiWidget* root = findWidget(root_id);
  if (target == nullptr || root == nullptr || !target->visible ||
      tooltip_dismissed_ || tooltip_seconds_ < GUI_TOOLTIP_DELAY_SECONDS) {
    return;
  }
  drawTooltip(ctx.themedBy(themeAt(tooltip_target_)), *target, root->rect);
}

}  // namespace eng
