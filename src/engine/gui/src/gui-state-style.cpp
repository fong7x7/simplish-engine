#include <engine/gui/gui-state-style.h>
#include <engine/gui/gui-state-styles.h>

namespace eng {

GuiStateStyle GuiStateStyle::lerp(const GuiStateStyle& a,
                                  const GuiStateStyle& b, float t) {
  return {.fill = GuiColor::lerp(a.fill, b.fill, t),
          .text = GuiColor::lerp(a.text, b.text, t),
          .border = GuiColor::lerp(a.border, b.border, t),
          .border_width =
              a.border_width + (b.border_width - a.border_width) * t,
          .radius = a.radius + (b.radius - a.radius) * t,
          .elevation = t < 0.5f ? a.elevation : b.elevation};
}

const GuiStateStyle& GuiStateStyles::of(GuiWidgetState state) const {
  return states[static_cast<size_t>(state)];
}

GuiStateStyle& GuiStateStyles::of(GuiWidgetState state) {
  return states[static_cast<size_t>(state)];
}

GuiStateStyles GuiStateStyles::uniform(const GuiStateStyle& style) {
  GuiStateStyles out;
  out.states.fill(style);
  return out;
}

}  // namespace eng
