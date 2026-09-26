#include <engine/gui/gui-checkbox.h>
#include <engine/gui/gui-draw-context.h>

namespace eng {

namespace {

  /// The box's side, and the gap before the label.
  constexpr float BOX = 16.0f;
  constexpr float GAP = 8.0f;

  /// Draw a stroke from (@p x0, @p y0) to (@p x1, @p y1).
  void stroke(const GuiDrawContext& ctx, const Rect& seg, GuiColor color) {
    if (ctx.renderer != nullptr) {
      ctx.renderer->emitLine(
          {seg.x, seg.y, seg.x + seg.w, seg.y + seg.h, color.pack(), 2.0f});
    }
  }

  /// The tick, or the dash, inside @p box.
  void drawMark(const GuiDrawContext& ctx, const Rect& box, GuiCheckState state,
                GuiColor color) {
    if (state == GuiCheckState::INDETERMINATE) {
      stroke(ctx, {box.x + 4.0f, box.y + 8.0f, 8.0f, 0.0f}, color);
      return;
    }
    stroke(ctx, {box.x + 3.5f, box.y + 8.5f, 3.0f, 3.0f}, color);
    stroke(ctx, {box.x + 6.5f, box.y + 11.5f, 6.0f, -7.0f}, color);
  }

}  // namespace

GuiCheckbox::GuiCheckbox() {
  tree_focusable = true;
}

std::unique_ptr<GuiWidget> GuiCheckbox::clone() const {
  return std::make_unique<GuiCheckbox>(*this);
}

void GuiCheckbox::render(const GuiDrawContext& ctx) const {
  const GuiTheme& t = ctx.activeTheme();
  const GuiPalette& p = t.palette;
  const bool on = state != GuiCheckState::UNCHECKED;
  const Rect box{rect.x, rect.y + (rect.h - BOX) * 0.5f, BOX, BOX};
  GuiStateStyle look = t.field.of(visualState());
  look.fill = on ? p.primary : look.fill;
  look.border = on ? p.primary : look.border;
  ctx.drawBox(box, look, opacity);
  if (on) {
    drawMark(ctx, box, state, GuiColor::applyOpacity(p.on_primary, opacity));
  }
  drawLabel(ctx, box.x + BOX + GAP);
}

void GuiCheckbox::drawLabel(const GuiDrawContext& ctx, float x) const {
  const GuiTheme& t = ctx.activeTheme();
  const GuiFont& font = t.font(GuiTextRole::BODY);
  const float lh = ctx.fontMetrics(font).line_height;
  ctx.drawText(
      {.text = label,
       .pos = {x, rect.y + (rect.h - lh) * 0.5f},
       .color = GuiColor::applyOpacity(
           disabled ? t.palette.text_disabled : t.palette.text, opacity),
       .font = font});
}

LayoutSize GuiCheckbox::measureContent(const GuiDrawContext& ctx,
                                       float /*max_width*/) const {
  const GuiFont& font = ctx.activeTheme().font(GuiTextRole::BODY);
  const float text = label.empty() ? 0.0f : GAP + ctx.measureText(label, font);
  return {BOX + text, std::max(BOX, ctx.fontMetrics(font).line_height)};
}

bool GuiCheckbox::handleClick(const GuiMouseEvent& event) {
  if (disabled) {
    return false;
  }
  toggle();
  (void)GuiWidget::handleClick(event);
  return true;
}

bool GuiCheckbox::handleNav(GuiNavCommand command) {
  if (disabled || command != GuiNavCommand::CONFIRM) {
    return false;
  }
  toggle();
  return true;
}

void GuiCheckbox::toggle() {
  state = state == GuiCheckState::CHECKED ? GuiCheckState::UNCHECKED
                                          : GuiCheckState::CHECKED;
  if (on_change) {
    on_change(state == GuiCheckState::CHECKED);
  }
}

}  // namespace eng
