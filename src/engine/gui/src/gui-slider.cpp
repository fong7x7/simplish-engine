#include "engine/gui/gui-slider.h"

#include "engine/gui/gui-draw-context.h"
#include "engine/gui/gui-style.h"

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

GuiSlider::ResolvedColors GuiSlider::resolveStyle() const {
  if (!hasSharedStyle()) {
    auto hc = hovered ? style.handle_hover_color : style.handle_color;
    return {style.track_color, style.fill_color, hc, style.track_height,
            style.handle_size};
  }
  auto hc = hovered ? ui_style->slider_handle_hover : ui_style->slider_handle;
  return {ui_style->slider_track, ui_style->slider_fill, hc,
          ui_style->slider_track_height, ui_style->slider_handle_size};
}

void GuiSlider::render(const GuiDrawContext& ctx) const {
  auto [track_col, fill_col, handle_col, th, hs] = resolveStyle();
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
  float prev = value;
  value = (rect.w > NORM_MIN) ? (mx - rect.x) / rect.w : NORM_MIN;
  value = std::clamp(value, NORM_MIN, NORM_MAX);
  if (value != prev && on_change) {
    on_change(mappedValue());
  }
}

}  // namespace eng
