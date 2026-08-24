#include "engine/gui/gui-label.h"

#include "engine/gui/gui-draw-context.h"
#include "engine/gui/gui-style.h"

namespace eng {

std::unique_ptr<GuiWidget> GuiLabel::clone() const {
  return std::make_unique<GuiLabel>(*this);
}

/// Half multiplier for computing center coordinate.
constexpr float HALF = 0.5f;

void GuiLabel::render(const GuiDrawContext& ctx) const {
  const bool use_shared = hasSharedStyle();
  auto c = GuiColor::applyOpacity(use_shared ? ui_style->text : color, opacity);
  switch (align) {
    case GuiLabelAlign::LEFT:
      ctx.drawText(c, drawPosInset(rect, 0, 0), text);
      break;
    case GuiLabelAlign::CENTER:
      ctx.drawCenteredText(rect, c, text);
      break;
    case GuiLabelAlign::H_CENTER: {
      const int cx = static_cast<int>(rect.x + rect.w * HALF);
      ctx.drawHCenteredText({c, cx, static_cast<int>(rect.y), text});
      break;
    }
  }
}

}  // namespace eng
