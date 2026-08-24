#include "engine/gui/gui-panel.h"

#include "engine/gui/gui-draw-context.h"
#include "engine/gui/gui-style.h"

namespace eng {

std::unique_ptr<GuiWidget> GuiPanel::clone() const {
  return std::make_unique<GuiPanel>(*this);
}

void GuiPanel::renderPanel(const RenderPanelParams& params) const {
  const auto& ctx = params.ctx;
  auto fill = GuiColor::applyOpacity(params.fill, opacity);
  if (corner_radius > 0.0f) {
    ctx.drawRoundedRect(rect, fill, corner_radius);
  } else {
    ctx.drawFilledRect(rect, fill);
  }
  if (border_width > 0.0f) {
    auto bc = GuiColor::applyOpacity(border_color, opacity);
    ctx.drawRoundedBorderRect({rect, bc, corner_radius, border_width});
  }
}

void GuiPanel::render(const GuiDrawContext& ctx) const {
  const bool use_shared = hasSharedStyle();
  auto color = use_shared ? ui_style->panel : fill_color;
  renderPanel({ctx, color});
}

}  // namespace eng
