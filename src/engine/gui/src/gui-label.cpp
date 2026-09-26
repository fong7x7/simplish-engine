#include "engine/gui/gui-label.h"

#include "engine/gui/gui-draw-context.h"

#include <algorithm>
#include <limits>
#include <string>

namespace eng {

namespace {

  /// No width limit, for text that breaks only at newlines.
  constexpr float UNLIMITED = std::numeric_limits<float>::max();

  /// Where a line @p width wide starts in @p rect, aligned by @p align.
  float lineX(const Rect& rect, GuiLabelAlign align, float width) {
    switch (align) {
      case GuiLabelAlign::CENTER:
      case GuiLabelAlign::H_CENTER:
        return rect.x + (rect.w - width) * 0.5f;
      case GuiLabelAlign::RIGHT:
        return rect.x + rect.w - width;
      case GuiLabelAlign::LEFT:
        return rect.x;
    }
    return rect.x;
  }

}  // namespace

std::unique_ptr<GuiWidget> GuiLabel::clone() const {
  return std::make_unique<GuiLabel>(*this);
}

GuiFont GuiLabel::resolvedFont(const GuiDrawContext& ctx) const {
  return font.value_or(ctx.activeTheme().font(role));
}

LayoutSize GuiLabel::measureContent(const GuiDrawContext& ctx,
                                    float max_width) const {
  const GuiFont f = resolvedFont(ctx);
  const float limit =
      wrap == GuiTextWrap::WORD && max_width >= 0.0f ? max_width : UNLIMITED;
  float widest = 0.0f;
  const auto lines = ctx.wrapText(text, f, limit);
  for (const GuiWrappedLine& line : lines) {
    widest = std::max(widest, line.width);
  }
  return {widest,
          ctx.fontMetrics(f).line_height * static_cast<float>(lines.size())};
}

void GuiLabel::render(const GuiDrawContext& ctx) const {
  const GuiFont f = resolvedFont(ctx);
  const GuiColor c = GuiColor::applyOpacity(
      color.value_or(ctx.activeTheme().palette.text), opacity);
  const auto lines =
      ctx.wrapText(text, f, wrap == GuiTextWrap::WORD ? rect.w : UNLIMITED);
  const float lh = ctx.fontMetrics(f).line_height;
  const float block = lh * static_cast<float>(lines.size());
  float y = align == GuiLabelAlign::CENTER ? rect.y + (rect.h - block) * 0.5f
                                           : rect.y;
  for (const GuiWrappedLine& line : lines) {
    drawLine(ctx, line, {.pos = {0.0f, y}, .color = c, .font = f});
    y += lh;
  }
}

void GuiLabel::drawLine(const GuiDrawContext& ctx, const GuiWrappedLine& line,
                        const GuiTextDraw& style) const {
  std::string shown(text.substr(line.begin, line.end - line.begin));
  if (overflow == GuiTextOverflow::ELLIPSIS && line.width > rect.w) {
    shown = ctx.ellipsize(shown, style.font, rect.w);
  }
  GuiTextDraw draw = style;
  draw.text = shown;
  draw.pos.x = lineX(rect, align, ctx.measureText(shown, style.font));
  ctx.drawText(draw);
}

}  // namespace eng
