#include "engine/gui/gui-slider.h"

#include "engine/gui/gui-draw-context.h"

#include <algorithm>

namespace eng {

std::unique_ptr<GuiWidget> GuiSlider::clone() const {
  return std::make_unique<GuiSlider>(*this);
}

namespace {
  /// Multiplier for computing centered offsets and corner radii.
  constexpr float HALF = 0.5f;
  /// Minimum bound of the normalized slider range.
  constexpr float NORM_MIN = 0.0f;
  /// Maximum bound of the normalized slider range.
  constexpr float NORM_MAX = 1.0f;
}  // namespace

GuiSlider::ResolvedColors
GuiSlider::resolveStyle(const GuiDrawContext& ctx) const {
  const GuiPalette& p = ctx.activeTheme().palette;
  const GuiColor handle = (hovered || pressed)
                              ? style.handle_hover_color.value_or(p.on_primary)
                              : style.handle_color.value_or(p.text);
  return {style.track_color.value_or(p.control),
          disabled ? p.text_disabled : style.fill_color.value_or(p.primary),
          disabled ? p.text_disabled : handle, style.track_height,
          style.handle_size};
}

void GuiSlider::render(const GuiDrawContext& ctx) const {
  auto [track_col, fill_col, handle_col, th, hs] = resolveStyle(ctx);
  auto tc = GuiColor::applyOpacity(track_col, opacity);
  auto fc = GuiColor::applyOpacity(fill_col, opacity);
  auto hc = GuiColor::applyOpacity(handle_col, opacity);
  float clamped = std::clamp(value, NORM_MIN, NORM_MAX);
  float track_y = rect.y + (rect.h - th) * HALF;
  float radius = th * HALF;

  ctx.drawRoundedRect({rect.x, track_y, rect.w, th}, tc, radius);
  ctx.drawRoundedRect({rect.x, track_y, clamped * rect.w, th}, fc, radius);

  float hx = rect.x + clamped * rect.w - hs * HALF;
  float hy = rect.y + (rect.h - hs) * HALF;
  ctx.drawRoundedRect({hx, hy, hs, hs}, hc, hs * HALF);
}

bool GuiSlider::handleMouseDown(const GuiMouseEvent& event) {
  if (disabled) {
    return false;
  }
  updateValueFromX(event.x);
  return true;
}

void GuiSlider::handleMouseMove(const GuiMouseEvent& event) {
  updateValueFromX(event.x);
}

void GuiSlider::handleMouseUp(const GuiMouseEvent& /*event*/) {}

float GuiSlider::mappedValue() const {
  float clamped = std::clamp(value, NORM_MIN, NORM_MAX);
  return min_value + clamped * (max_value - min_value);
}

void GuiSlider::setMappedValue(float mapped) {
  float range = max_value - min_value;
  value = (range != NORM_MIN) ? (mapped - min_value) / range : NORM_MIN;
  value = std::clamp(value, NORM_MIN, NORM_MAX);
}

void GuiSlider::updateValueFromX(float mx) {
  setNormalized((rect.w > NORM_MIN) ? (mx - rect.x) / rect.w : NORM_MIN);
}

void GuiSlider::setNormalized(float normalized) {
  const float prev = value;
  value = std::clamp(normalized, NORM_MIN, NORM_MAX);
  if (value != prev && on_change) {
    on_change(mappedValue());
  }
}

GuiSlider::GuiSlider() {
  tree_focusable = true;
}

bool GuiSlider::handleNav(GuiNavCommand command) {
  if (command == GuiNavCommand::LEFT) {
    setNormalized(value - nav_step);
    return true;
  }
  if (command == GuiNavCommand::RIGHT) {
    setNormalized(value + nav_step);
    return true;
  }
  return false;
}

}  // namespace eng
