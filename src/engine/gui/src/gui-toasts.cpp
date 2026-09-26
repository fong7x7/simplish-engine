#include <algorithm>
#include <engine/gui/gui-draw-context.h>
#include <engine/gui/gui-toasts.h>
#include <ranges>
#include <utility>

namespace eng {

namespace {

  /// Space from the corner, between toasts, and inside one.
  constexpr float EDGE = 16.0f;
  constexpr float SPACING = 8.0f;
  constexpr float PAD_X = 14.0f;
  constexpr float PAD_Y = 10.0f;
  /// Width of the coloured rule down a toast's left side.
  constexpr float ACCENT = 3.0f;
  /// Seconds a toast takes to fade in and to fade out.
  constexpr float FADE_IN = 0.15f;
  constexpr float FADE_OUT = 0.3f;

  /// How visible @p toast is now: rising as it arrives, falling as it goes.
  float presence(const GuiToast& toast) {
    const float in = std::min(1.0f, toast.age / FADE_IN);
    const float out = std::min(1.0f, (toast.lifetime - toast.age) / FADE_OUT);
    return std::clamp(std::min(in, out), 0.0f, 1.0f);
  }

  GuiColor accentOf(GuiToastKind kind, const GuiPalette& p) {
    switch (kind) {
      case GuiToastKind::SUCCESS:
        return p.success;
      case GuiToastKind::WARNING:
        return p.warning;
      case GuiToastKind::ERROR:
        return p.danger;
      case GuiToastKind::INFO:
        return p.primary;
    }
    return p.primary;
  }

  /// Paint @p toast's box and accent rule at @p box, faded to @p alpha.
  void drawToastBox(const GuiDrawContext& ctx, const GuiToast& toast,
                    const Rect& box, float alpha) {
    const GuiTheme& theme = ctx.activeTheme();
    const float r = theme.menu.radius;
    ctx.drawBox(box, theme.menu, alpha);
    ctx.drawRect({.rect = {box.x, box.y, ACCENT, box.h},
                  .fill = GuiColor::applyOpacity(
                      accentOf(toast.kind, theme.palette), alpha),
                  .radii = GuiCorners{r, 0.0f, 0.0f, r}});
  }

  /// Draw @p toast with its box's bottom-right corner at @p corner; the
  /// height it took.
  float drawToast(const GuiDrawContext& ctx, const GuiToast& toast,
                  const DrawPos& corner, float max_width) {
    const GuiTheme& theme = ctx.activeTheme();
    const GuiFont& font = theme.font(GuiTextRole::BODY);
    const float alpha = presence(toast);
    const std::string text =
        ctx.ellipsize(toast.text, font, max_width - PAD_X * 2 - ACCENT);
    const float w = ctx.measureText(text, font) + PAD_X * 2 + ACCENT;
    const float h = ctx.fontMetrics(font).line_height + PAD_Y * 2;
    // Slides up a little as it arrives.
    const Rect box{corner.x - w, corner.y - h + (1.0f - alpha) * 8.0f, w, h};
    drawToastBox(ctx, toast, box, alpha);
    ctx.drawText({.text = text,
                  .pos = {box.x + ACCENT + PAD_X, box.y + PAD_Y},
                  .color = GuiColor::applyOpacity(theme.palette.text, alpha),
                  .font = font});
    return h * alpha;
  }


}  // namespace

GuiToasts::GuiToasts() {
  debug_name = "gui-toasts";
  pointer_through = true;
  tree_layout.position = PositionMode::ABSOLUTE;
  tree_layout.abs_right = 0.0f;
  tree_layout.abs_bottom = 0.0f;
}

std::unique_ptr<GuiWidget> GuiToasts::clone() const {
  return std::make_unique<GuiToasts>(*this);
}

void GuiToasts::update(const GuiDrawContext& ctx, float dt) {
  GuiWidget::update(ctx, dt);
  for (GuiToast& toast : toasts_) {
    toast.age += dt;
  }
  std::erase_if(toasts_, [](const GuiToast& t) { return t.age >= t.lifetime; });
}

void GuiToasts::render(const GuiDrawContext& ctx) const {
  float bottom = rect.y + rect.h - EDGE;
  const float right = rect.x + rect.w - EDGE;
  for (const GuiToast& toast : std::views::reverse(toasts_)) {
    bottom -= drawToast(ctx, toast, {right, bottom}, max_width) + SPACING;
  }
}

void GuiToasts::show(std::string text, GuiToastKind kind, float seconds) {
  toasts_.push_back({std::move(text), kind, 0.0f, seconds});
  if (toasts_.size() > max_shown) {
    toasts_.erase(toasts_.begin());
  }
}

const std::vector<GuiToast>& GuiToasts::shown() const {
  return toasts_;
}

}  // namespace eng
