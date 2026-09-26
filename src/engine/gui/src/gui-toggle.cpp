#include <algorithm>
#include <engine/gui/gui-draw-context.h>
#include <engine/gui/gui-toggle.h>

namespace eng {

namespace {

  /// The switch's size, its knob's inset, and the gap after the label.
  constexpr float TRACK_W = 34.0f;
  constexpr float TRACK_H = 20.0f;
  constexpr float KNOB_INSET = 2.0f;
  constexpr float GAP = 10.0f;
  /// Seconds the knob takes to cross.
  constexpr float SLIDE_SECONDS = 0.14f;

}  // namespace

GuiToggle::GuiToggle() {
  tree_focusable = true;
}

std::unique_ptr<GuiWidget> GuiToggle::clone() const {
  return std::make_unique<GuiToggle>(*this);
}

void GuiToggle::update(const GuiDrawContext& ctx, float dt) {
  GuiWidget::update(ctx, dt);
  const float target = on ? 1.0f : 0.0f;
  const float step = dt / SLIDE_SECONDS;
  knob_ = knob_ < target ? std::min(target, knob_ + step)
                         : std::max(target, knob_ - step);
}

void GuiToggle::render(const GuiDrawContext& ctx) const {
  const GuiPalette& p = ctx.activeTheme().palette;
  const GuiFont& font = ctx.activeTheme().font(GuiTextRole::BODY);
  const float lh = ctx.fontMetrics(font).line_height;
  ctx.drawText({.text = label,
                .pos = {rect.x, rect.y + (rect.h - lh) * 0.5f},
                .color = GuiColor::applyOpacity(
                    disabled ? p.text_disabled : p.text, opacity),
                .font = font});
  drawSwitch(ctx, {rect.x + rect.w - TRACK_W,
                   rect.y + (rect.h - TRACK_H) * 0.5f, TRACK_W, TRACK_H});
}

void GuiToggle::drawSwitch(const GuiDrawContext& ctx, const Rect& track) const {
  const GuiPalette& p = ctx.activeTheme().palette;
  const GuiColor fill =
      GuiColor::lerp(hovered ? p.control_hover : p.control, p.primary, knob_);
  ctx.drawRoundedRect(track, GuiColor::applyOpacity(fill, opacity),
                      TRACK_H * 0.5f);
  const float knob = TRACK_H - KNOB_INSET * 2.0f;
  const float x = track.x + KNOB_INSET + knob_ * (TRACK_W - TRACK_H);
  ctx.drawRoundedRect({x, track.y + KNOB_INSET, knob, knob},
                      GuiColor::applyOpacity(p.on_primary, opacity),
                      knob * 0.5f);
}

LayoutSize GuiToggle::measureContent(const GuiDrawContext& ctx,
                                     float /*max_width*/) const {
  const GuiFont& font = ctx.activeTheme().font(GuiTextRole::BODY);
  const float text = label.empty() ? 0.0f : ctx.measureText(label, font) + GAP;
  return {text + TRACK_W, std::max(TRACK_H, ctx.fontMetrics(font).line_height)};
}

bool GuiToggle::handleClick(const GuiMouseEvent& event) {
  if (disabled) {
    return false;
  }
  flip();
  (void)GuiWidget::handleClick(event);
  return true;
}

bool GuiToggle::handleNav(GuiNavCommand command) {
  if (disabled) {
    return false;
  }
  const bool turn = command == GuiNavCommand::CONFIRM ||
                    (command == GuiNavCommand::LEFT && on) ||
                    (command == GuiNavCommand::RIGHT && !on);
  if (turn) {
    flip();
  }
  return turn || command == GuiNavCommand::LEFT ||
         command == GuiNavCommand::RIGHT;
}

void GuiToggle::flip() {
  on = !on;
  if (on_change) {
    on_change(on);
  }
}

}  // namespace eng
