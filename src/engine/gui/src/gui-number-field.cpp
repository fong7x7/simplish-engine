#include <algorithm>
#include <cmath>
#include <cstdio>
#include <engine/gui/gui-draw-context.h>
#include <engine/gui/gui-number-field.h>

namespace eng {

namespace {

  /// Width of each stepper, and padding either side of the value.
  constexpr float STEPPER = 22.0f;
  constexpr float PAD = 6.0f;

  /// @p v rounded to @p decimals places.
  double roundTo(double v, int decimals) {
    const double k = std::pow(10.0, decimals);
    return std::round(v * k) / k;
  }

}  // namespace

GuiNumberField::GuiNumberField() {
  tree_focusable = true;
}

std::unique_ptr<GuiWidget> GuiNumberField::clone() const {
  return std::make_unique<GuiNumberField>(*this);
}

const GuiStateStyles* GuiNumberField::themeStyles(const GuiTheme& theme) const {
  return &theme.field;
}

std::string GuiNumberField::text() const {
  char buffer[64];
  (void)std::snprintf(buffer, sizeof(buffer), "%.*f", std::max(0, decimals),
                      value);
  return std::string(buffer) + suffix;
}

void GuiNumberField::render(const GuiDrawContext& ctx) const {
  const GuiStateStyle look = drawnStyle(ctx);
  ctx.drawBox(rect, look, opacity);
  const GuiTheme& t = ctx.activeTheme();
  const GuiFont& font = t.font(GuiTextRole::BODY);
  const float lh = ctx.fontMetrics(font).line_height;
  const float y = rect.y + (rect.h - lh) * 0.5f;
  drawSteppers(ctx, y);
  const std::string shown = text();
  const float w = ctx.measureText(shown, font);
  ctx.drawText({.text = shown,
                .pos = {rect.x + (rect.w - w) * 0.5f, y},
                .color = GuiColor::applyOpacity(look.text, opacity),
                .font = font});
}

void GuiNumberField::drawSteppers(const GuiDrawContext& ctx, float y) const {
  const GuiTheme& t = ctx.activeTheme();
  const GuiFont& font = t.font(GuiTextRole::BODY);
  const GuiColor muted = GuiColor::applyOpacity(t.palette.text_muted, opacity);
  ctx.drawText({.text = "\xE2\x88\x92",  // −
                .pos = {rect.x + 7.0f, y},
                .color = muted,
                .font = font});
  ctx.drawText({.text = "+",
                .pos = {rect.x + rect.w - STEPPER + 6.0f, y},
                .color = muted,
                .font = font});
}

LayoutSize GuiNumberField::measureContent(const GuiDrawContext& ctx,
                                          float /*max_width*/) const {
  const GuiFont& font = ctx.activeTheme().font(GuiTextRole::BODY);
  GuiNumberField widest = *this;
  widest.value = std::abs(min) > std::abs(max) ? min : max;
  return {ctx.measureText(widest.text(), font) + (STEPPER + PAD) * 2.0f,
          ctx.fontMetrics(font).line_height + PAD * 2.0f};
}

bool GuiNumberField::handleMouseDown(const GuiMouseEvent& event) {
  if (disabled) {
    return false;
  }
  if (event.x < rect.x + STEPPER) {
    setValue(value - step);
  } else if (event.x > rect.x + rect.w - STEPPER) {
    setValue(value + step);
  } else {
    scrubbing_ = true;
    scrub_x_ = event.x;
    scrub_from_ = value;
  }
  return true;
}

void GuiNumberField::handleMouseMove(const GuiMouseEvent& event) {
  if (scrubbing_ && pixels_per_step > 0.0f) {
    const double steps = std::trunc((event.x - scrub_x_) / pixels_per_step);
    setValue(scrub_from_ + steps * step);
  }
  GuiWidget::handleMouseMove(event);
}

void GuiNumberField::handleMouseUp(const GuiMouseEvent& event) {
  scrubbing_ = false;
  GuiWidget::handleMouseUp(event);
}

bool GuiNumberField::handleScroll(const GuiScrollEvent& event) {
  if (disabled || event.delta_y == 0.0f) {
    return false;
  }
  setValue(value + (event.delta_y > 0.0f ? step : -step));
  return true;
}

bool GuiNumberField::handleNav(GuiNavCommand command) {
  if (disabled ||
      (command != GuiNavCommand::LEFT && command != GuiNavCommand::RIGHT)) {
    return false;
  }
  setValue(value + (command == GuiNavCommand::RIGHT ? step : -step));
  return true;
}

void GuiNumberField::setValue(double v) {
  const double next = roundTo(std::clamp(v, min, max), decimals);
  if (next == value) {
    return;
  }
  value = next;
  if (on_change) {
    on_change(value);
  }
}

}  // namespace eng
