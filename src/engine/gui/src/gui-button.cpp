#include "engine/gui/gui-button.h"

#include "engine/gui/gui-draw-context.h"

namespace eng {

GuiButton::GuiButton() {
  widget_type = GuiWidgetType::BUTTON;
  tree_focusable = true;
}

std::unique_ptr<GuiWidget> GuiButton::clone() const {
  return std::make_unique<GuiButton>(*this);
}

LayoutSize GuiButton::measureContent(const GuiDrawContext& ctx,
                                     float /*max_width*/) const {
  const GuiFont& font = ctx.activeTheme().font(role);
  return {ctx.measureText(label, font), ctx.fontMetrics(font).line_height};
}

const GuiStateStyles* GuiButton::themeStyles(const GuiTheme& theme) const {
  return &theme.button(variant);
}

void GuiButton::render(const GuiDrawContext& ctx) const {
  const GuiStateStyle style = drawnStyle(ctx);
  ctx.drawBox(rect, style, opacity);
  const GuiFont& font = ctx.activeTheme().font(role);
  const float w = ctx.measureText(label, font);
  const float h = ctx.fontMetrics(font).line_height;
  ctx.drawText(
      {.text = label,
       .pos = {rect.x + (rect.w - w) * 0.5f, rect.y + (rect.h - h) * 0.5f},
       .color = GuiColor::applyOpacity(style.text, opacity),
       .font = font});
}

}  // namespace eng
