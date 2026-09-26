#include <engine/gui/gui-draw-context.h>
#include <engine/gui/gui-radio-group.h>
#include <utility>

namespace eng {

namespace {

  /// The circle's diameter and the gap before its label.
  constexpr float DOT = 16.0f;
  constexpr float GAP = 8.0f;

  /// What an option's row needs to draw itself.
  struct OptionRow {
    /// Its rect.
    Rect rect;
    /// Its label.
    const std::string& label;
    /// Whether it is the chosen one.
    bool chosen = false;
    /// Whether the pointer is on it.
    bool lit = false;
  };

  void drawOption(const GuiDrawContext& ctx, const OptionRow& row,
                  float opacity) {
    const GuiTheme& t = ctx.activeTheme();
    const Rect dot{row.rect.x, row.rect.y + (row.rect.h - DOT) * 0.5f, DOT,
                   DOT};
    GuiStateStyle ring =
        t.field.of(row.lit ? GuiWidgetState::HOVER : GuiWidgetState::NORMAL);
    ring.radius = DOT * 0.5f;
    ring.border = row.chosen ? t.palette.primary : ring.border;
    ring.border_width = row.chosen ? 5.0f : ring.border_width;
    ctx.drawBox(dot, ring, opacity);
    const GuiFont& font = t.font(GuiTextRole::BODY);
    const float lh = ctx.fontMetrics(font).line_height;
    ctx.drawText(
        {.text = row.label,
         .pos = {dot.x + DOT + GAP, row.rect.y + (row.rect.h - lh) * 0.5f},
         .color = GuiColor::applyOpacity(t.palette.text, opacity),
         .font = font});
  }

}  // namespace

GuiRadioGroup::GuiRadioGroup() {
  tree_focusable = true;
}

std::unique_ptr<GuiWidget> GuiRadioGroup::clone() const {
  return std::make_unique<GuiRadioGroup>(*this);
}

void GuiRadioGroup::render(const GuiDrawContext& ctx) const {
  for (size_t i = 0; i < options.size(); ++i) {
    const auto index = static_cast<int>(i);
    const Rect row{rect.x, rect.y + row_height * static_cast<float>(i), rect.w,
                   row_height};
    drawOption(ctx,
               {row, options[i], index == selected, index == hovered_option_},
               opacity);
  }
}

LayoutSize GuiRadioGroup::measureContent(const GuiDrawContext& ctx,
                                         float /*max_width*/) const {
  const GuiFont& font = ctx.activeTheme().font(GuiTextRole::BODY);
  float widest = 0.0f;
  for (const std::string& option : options) {
    widest = std::max(widest, ctx.measureText(option, font));
  }
  return {DOT + GAP + widest, row_height * static_cast<float>(options.size())};
}

int GuiRadioGroup::optionAt(float x, float y) const {
  if (!containsPoint(rect, x, y) || row_height <= 0.0f) {
    return -1;
  }
  const auto row = static_cast<int>((y - rect.y) / row_height);
  return std::cmp_less(row, options.size()) ? row : -1;
}

bool GuiRadioGroup::handleClick(const GuiMouseEvent& event) {
  const int at = optionAt(event.x, event.y);
  if (disabled || at < 0) {
    return false;
  }
  choose(at);
  return true;
}

void GuiRadioGroup::handleMouseMove(const GuiMouseEvent& event) {
  hovered_option_ = optionAt(event.x, event.y);
  GuiWidget::handleMouseMove(event);
}

bool GuiRadioGroup::handleNav(GuiNavCommand command) {
  const int count = static_cast<int>(options.size());
  if (disabled || count == 0) {
    return false;
  }
  if (command == GuiNavCommand::DOWN && selected + 1 < count) {
    choose(selected + 1);
    return true;
  }
  if (command == GuiNavCommand::UP && selected > 0) {
    choose(selected - 1);
    return true;
  }
  return false;
}

void GuiRadioGroup::choose(int index) {
  if (index == selected || index < 0 ||
      std::cmp_greater_equal(index, options.size())) {
    return;
  }
  selected = index;
  if (on_change) {
    on_change(index);
  }
}

}  // namespace eng
