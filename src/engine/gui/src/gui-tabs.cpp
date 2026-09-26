#include <algorithm>
#include <engine/gui/gui-draw-context.h>
#include <engine/gui/gui-tabs.h>
#include <utility>

namespace eng {

namespace {

  /// Padding across a tab and up and down, and the underline's thickness.
  constexpr float PAD_X = 14.0f;
  constexpr float PAD_Y = 8.0f;
  constexpr float UNDERLINE = 2.0f;
  /// How fast the underline catches up: the share of the way per second.
  constexpr float SLIDE_RATE = 18.0f;

  /// Each of @p tabs' left edge and width, from @p left.
  std::vector<std::pair<float, float>>
  tabSpans(const GuiDrawContext& ctx, const std::vector<std::string>& tabs,
           float left) {
    const GuiFont& font = ctx.activeTheme().font(GuiTextRole::LABEL);
    std::vector<std::pair<float, float>> spans;
    for (const std::string& tab : tabs) {
      const float w = ctx.measureText(tab, font) + PAD_X * 2.0f;
      spans.emplace_back(left, w);
      left += w;
    }
    return spans;
  }

  float approach(float from, float to, float k) {
    return from + (to - from) * k;
  }

}  // namespace

GuiTabs::GuiTabs() {
  tree_focusable = true;
}

std::unique_ptr<GuiWidget> GuiTabs::clone() const {
  return std::make_unique<GuiTabs>(*this);
}

void GuiTabs::update(const GuiDrawContext& ctx, float dt) {
  GuiWidget::update(ctx, dt);
  spans_ = tabSpans(ctx, tabs, rect.x);
  if (selected < 0 || std::cmp_greater_equal(selected, spans_.size())) {
    return;
  }
  const auto target = spans_[static_cast<size_t>(selected)];
  const float k =
      underline_.second <= 0.0f ? 1.0f : std::min(1.0f, dt * SLIDE_RATE);
  underline_ = {approach(underline_.first, target.first, k),
                approach(underline_.second, target.second, k)};
}

void GuiTabs::render(const GuiDrawContext& ctx) const {
  const GuiTheme& t = ctx.activeTheme();
  const GuiFont& font = t.font(GuiTextRole::LABEL);
  const auto spans = tabSpans(ctx, tabs, rect.x);
  for (size_t i = 0; i < tabs.size(); ++i) {
    const auto index = static_cast<int>(i);
    const bool strong = index == selected || index == hovered_tab_;
    ctx.drawText({.text = tabs[i],
                  .pos = {spans[i].first + PAD_X, rect.y + PAD_Y},
                  .color = GuiColor::applyOpacity(
                      strong ? t.palette.text : t.palette.text_muted, opacity),
                  .font = font});
  }
  drawUnderline(ctx, spans);
}

void GuiTabs::drawUnderline(
    const GuiDrawContext& ctx,
    const std::vector<std::pair<float, float>>& spans) const {
  const GuiPalette& p = ctx.activeTheme().palette;
  const float base = rect.y + rect.h - 1.0f;
  ctx.drawFilledRect({rect.x, base, rect.w, 1.0f},
                     GuiColor::applyOpacity(p.border, opacity));
  // Before the first update there is no slide to follow: under the tab.
  const bool settled = underline_.second > 0.0f;
  const auto bar = settled || spans.empty()
                       ? underline_
                       : spans[static_cast<size_t>(std::max(selected, 0))];
  ctx.drawRoundedRect(
      {bar.first, base - UNDERLINE + 1.0f, bar.second, UNDERLINE},
      GuiColor::applyOpacity(p.primary, opacity), 1.0f);
}

LayoutSize GuiTabs::measureContent(const GuiDrawContext& ctx,
                                   float /*max_width*/) const {
  const auto spans = tabSpans(ctx, tabs, 0.0f);
  const float w =
      spans.empty() ? 0.0f : spans.back().first + spans.back().second;
  const GuiFont& font = ctx.activeTheme().font(GuiTextRole::LABEL);
  return {w, ctx.fontMetrics(font).line_height + PAD_Y * 2.0f + UNDERLINE};
}

int GuiTabs::tabAt(const GuiDrawContext& ctx, float x, float y) const {
  if (!containsPoint(rect, x, y)) {
    return -1;
  }
  const auto spans = tabSpans(ctx, tabs, rect.x);
  for (size_t i = 0; i < spans.size(); ++i) {
    if (x >= spans[i].first && x < spans[i].first + spans[i].second) {
      return static_cast<int>(i);
    }
  }
  return -1;
}

bool GuiTabs::handleClick(const GuiMouseEvent& event) {
  // The spans are the last update's: a click lands on what was drawn.
  for (size_t i = 0; i < spans_.size(); ++i) {
    if (event.x >= spans_[i].first &&
        event.x < spans_[i].first + spans_[i].second && !disabled) {
      select(static_cast<int>(i));
      return true;
    }
  }
  return false;
}

void GuiTabs::handleMouseMove(const GuiMouseEvent& event) {
  hovered_tab_ = -1;
  for (size_t i = 0; i < spans_.size(); ++i) {
    if (event.x >= spans_[i].first &&
        event.x < spans_[i].first + spans_[i].second) {
      hovered_tab_ = static_cast<int>(i);
    }
  }
  GuiWidget::handleMouseMove(event);
}

bool GuiTabs::handleNav(GuiNavCommand command) {
  const int count = static_cast<int>(tabs.size());
  if (disabled || count == 0) {
    return false;
  }
  if (command == GuiNavCommand::RIGHT && selected + 1 < count) {
    select(selected + 1);
    return true;
  }
  if (command == GuiNavCommand::LEFT && selected > 0) {
    select(selected - 1);
    return true;
  }
  return false;
}

void GuiTabs::select(int index) {
  if (index == selected || index < 0 ||
      std::cmp_greater_equal(index, tabs.size())) {
    return;
  }
  selected = index;
  if (on_change) {
    on_change(index);
  }
}

}  // namespace eng
