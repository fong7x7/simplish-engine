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

LayoutSize GuiButton::measureContent(const GuiDrawContext& ctx) const {
  return {ctx.measureText(label), ctx.textLineHeight()};
}

const GuiStateStyles* GuiButton::themeStyles(const GuiTheme& theme) const {
  return &theme.button(variant);
}

void GuiButton::render(const GuiDrawContext& ctx) const {
  const GuiStateStyle style = drawnStyle(ctx);
  ctx.drawBox(rect, style, opacity);
  ctx.drawCenteredText(rect, GuiColor::applyOpacity(style.text, opacity),
                       label);
}

}  // namespace eng
