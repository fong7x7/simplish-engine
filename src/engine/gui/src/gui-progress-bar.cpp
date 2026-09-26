#include <algorithm>
#include <cmath>
#include <engine/gui/gui-draw-context.h>
#include <engine/gui/gui-progress-bar.h>

namespace eng {

namespace {

  /// An indeterminate bar's sweeps a second, and its segment's share.
  constexpr float SWEEPS_PER_SECOND = 0.7f;
  constexpr float SEGMENT = 0.3f;

}  // namespace

GuiProgressBar::GuiProgressBar() = default;

std::unique_ptr<GuiWidget> GuiProgressBar::clone() const {
  return std::make_unique<GuiProgressBar>(*this);
}

void GuiProgressBar::update(const GuiDrawContext& ctx, float dt) {
  GuiWidget::update(ctx, dt);
  if (indeterminate) {
    phase_ = std::fmod(phase_ + dt * SWEEPS_PER_SECOND, 1.0f);
  }
}

void GuiProgressBar::render(const GuiDrawContext& ctx) const {
  const GuiPalette& p = ctx.activeTheme().palette;
  const float r = rect.h * 0.5f;
  ctx.drawRoundedRect(rect, GuiColor::applyOpacity(p.control, opacity), r);
  const GuiColor fill = GuiColor::applyOpacity(p.primary, opacity);
  if (indeterminate) {
    drawSweep(ctx, fill);
    return;
  }
  const float w = std::clamp(value, 0.0f, 1.0f) * rect.w;
  if (w > 0.0f) {
    ctx.drawRoundedRect({rect.x, rect.y, std::max(w, rect.h), rect.h}, fill, r);
  }
}

void GuiProgressBar::drawSweep(const GuiDrawContext& ctx,
                               const GuiColor& fill) const {
  // The segment enters from the left and leaves at the right.
  const float start = (phase_ * (1.0f + SEGMENT) - SEGMENT) * rect.w;
  const float x0 = std::max(rect.x, rect.x + start);
  const float x1 = std::min(rect.x + rect.w, rect.x + start + SEGMENT * rect.w);
  if (x1 > x0) {
    ctx.drawRoundedRect({x0, rect.y, x1 - x0, rect.h}, fill, rect.h * 0.5f);
  }
}

LayoutSize GuiProgressBar::measureContent(const GuiDrawContext& /*ctx*/,
                                          float /*max_width*/) const {
  return {0.0f, thickness};
}

}  // namespace eng
