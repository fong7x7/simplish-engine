#include <engine/gui/gui-card.h>

namespace eng {

std::unique_ptr<GuiWidget> GuiCard::clone() const {
  return std::make_unique<GuiCard>(*this);
}

const GuiStateStyles* GuiCard::themeStyles(const GuiTheme& theme) const {
  return &theme.card;
}

void GuiCard::render(const GuiDrawContext& ctx) const {
  GuiStateStyle style = drawnStyle(ctx);
  if (elevation) {
    style.elevation = *elevation;
  }
  ctx.drawBox(rect, style, opacity);
}

}  // namespace eng
