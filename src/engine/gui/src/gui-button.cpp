#include "engine/gui/gui-button.h"

#include "engine/gui/gui-draw-context.h"
#include "engine/gui/gui-style.h"

namespace eng {

std::unique_ptr<GuiWidget> GuiButton::clone() const {
  return std::make_unique<GuiButton>(*this);
}

void GuiButton::render(const GuiDrawContext& ctx) const {
  const bool use_shared = hasSharedStyle();
  auto bg = use_shared ? (hovered ? ui_style->btn_bg_hover : ui_style->btn_bg)
                       : (hovered ? style.hover_color : style.bg_color);
  auto tc = use_shared ? ui_style->btn_text : style.text_color;
  float rad = use_shared ? ui_style->btn_corner_radius : style.corner_radius;
  ctx.drawRoundedRect(rect, GuiColor::applyOpacity(bg, opacity), rad);
  if (border_width > 0.0f) {
    auto bc = GuiColor::applyOpacity(border_color, opacity);
    ctx.drawRoundedBorderRect({rect, bc, rad, border_width});
  }
  ctx.drawCenteredText(rect, GuiColor::applyOpacity(tc, opacity), label);
}

}  // namespace eng
