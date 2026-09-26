#include <engine/gui/gui-theme.h>
#include <utility>

namespace eng {

namespace {

  /// A variant's colours at rest, hovered, held and chosen, and its text.
  struct ButtonColors {
    /// At rest.
    GuiColor rest;
    /// Under the pointer.
    GuiColor hover;
    /// Held down.
    GuiColor pressed;
    /// Chosen: the active tool.
    GuiColor selected;
    /// Its text, at rest.
    GuiColor text;
    /// Its text when chosen.
    GuiColor selected_text;
  };

  GuiColor withAlpha(GuiColor color, uint8_t alpha) {
    color.a = alpha;
    return color;
  }

  /// A button's look in every state from @p c, with @p radius corners.
  GuiStateStyles buttonStyles(const GuiPalette& p, const ButtonColors& c,
                              float radius) {
    GuiStateStyles out = GuiStateStyles::uniform(
        {.fill = c.rest, .text = c.text, .radius = radius});
    out.of(GuiWidgetState::HOVER).fill = c.hover;
    out.of(GuiWidgetState::PRESSED).fill = c.pressed;
    out.of(GuiWidgetState::SELECTED).fill = c.selected;
    out.of(GuiWidgetState::SELECTED).text = c.selected_text;
    out.of(GuiWidgetState::DISABLED).fill = withAlpha(c.rest, c.rest.a / 2);
    out.of(GuiWidgetState::DISABLED).text = p.text_disabled;
    return out;
  }

  /// Every variant's colours, in `GuiButtonVariant` order.
  std::array<ButtonColors, GUI_BUTTON_VARIANT_COUNT>
  variantColors(const GuiPalette& p) {
    return {{{p.control, p.control_hover, p.control_pressed, p.primary, p.text,
              p.on_primary},
             {p.primary, p.primary_hover, p.primary_pressed, p.primary_pressed,
              p.on_primary, p.on_primary},
             {p.danger, p.danger_hover, p.danger, p.danger_hover, p.on_primary,
              p.on_primary},
             {withAlpha(p.control, 0), p.control_hover, p.control_pressed,
              p.control, p.text, p.text}}};
  }

  /// A text field: sunk in, a hairline border that strengthens under the
  /// pointer and takes the accent while typing.
  GuiStateStyles fieldStyles(const GuiPalette& p, float radius) {
    GuiStateStyles out = GuiStateStyles::uniform({.fill = p.surface_sunken,
                                                  .text = p.text,
                                                  .border = p.border,
                                                  .border_width = 1.0f,
                                                  .radius = radius});
    out.of(GuiWidgetState::HOVER).border = p.border_strong;
    out.of(GuiWidgetState::PRESSED).border = p.border_strong;
    out.of(GuiWidgetState::FOCUSED).border = p.primary;
    out.of(GuiWidgetState::SELECTED).border = p.primary;
    out.of(GuiWidgetState::DISABLED).text = p.text_disabled;
    return out;
  }

  /// A card: raised, lifting further under the pointer.
  GuiStateStyles cardStyles(const GuiPalette& p, float radius) {
    GuiStateStyles out =
        GuiStateStyles::uniform({.fill = p.surface_raised,
                                 .text = p.text,
                                 .border = p.border,
                                 .border_width = 1.0f,
                                 .radius = radius,
                                 .elevation = GuiElevation::LOW});
    out.of(GuiWidgetState::HOVER).elevation = GuiElevation::MID;
    out.of(GuiWidgetState::HOVER).border = p.border_strong;
    out.of(GuiWidgetState::SELECTED).border = p.primary;
    out.of(GuiWidgetState::DISABLED).text = p.text_disabled;
    return out;
  }

  /// The default shadows, darkening with @p p's shadow colour.
  std::array<GuiShadow, GUI_ELEVATION_COUNT>
  defaultShadows(const GuiPalette& p) {
    return {{{},
             {0.0f, 1.0f, 3.0f, 0.0f, p.shadow},
             {0.0f, 4.0f, 12.0f, 0.0f, p.shadow},
             {0.0f, 12.0f, 32.0f, 0.0f, p.shadow}}};
  }

  /// Each role's font from @p t's type scale, in `GuiTextRole` order.
  std::array<GuiFont, GUI_TEXT_ROLE_COUNT> roleFonts(const GuiTheme& t) {
    return {{{.size = t.textSize(GuiTextSize::SM)},
             {.size = t.textSize(GuiTextSize::MD), .weight = 500},
             {.size = t.textSize(GuiTextSize::MD)},
             {.size = t.textSize(GuiTextSize::LG), .weight = 600},
             {.size = t.textSize(GuiTextSize::XL), .weight = 600},
             {.size = t.textSize(GuiTextSize::DISPLAY),
              .weight = 700,
              .letter_spacing = -0.5f}}};
  }

}  // namespace

float GuiTheme::space(GuiSpace step) const {
  return spacing[static_cast<size_t>(step)];
}

float GuiTheme::radius(GuiRadius step) const {
  return radii[static_cast<size_t>(step)];
}

float GuiTheme::textSize(GuiTextSize step) const {
  return text_sizes[static_cast<size_t>(step)];
}

const GuiFont& GuiTheme::font(GuiTextRole role) const {
  return type_roles[static_cast<size_t>(role)];
}

const GuiShadow& GuiTheme::shadow(GuiElevation level) const {
  return shadows[static_cast<size_t>(level)];
}

const GuiStateStyles& GuiTheme::button(GuiButtonVariant variant) const {
  return buttons[static_cast<size_t>(variant)];
}

void GuiTheme::deriveComponents() {
  shadows = defaultShadows(palette);
  type_roles = roleFonts(*this);
  const auto colors = variantColors(palette);
  for (size_t i = 0; i < GUI_BUTTON_VARIANT_COUNT; ++i) {
    buttons[i] = buttonStyles(palette, colors[i], radius(GuiRadius::MD));
  }
  field = fieldStyles(palette, radius(GuiRadius::SM));
  card = cardStyles(palette, radius(GuiRadius::LG));
  menu = {.fill = palette.surface_raised,
          .text = palette.text,
          .border = palette.border,
          .border_width = 1.0f,
          .radius = radius(GuiRadius::MD),
          .elevation = GuiElevation::MID};
}

GuiTheme GuiTheme::fromPalette(const GuiPalette& palette, std::string name) {
  GuiTheme theme;
  theme.name = std::move(name);
  theme.palette = palette;
  theme.deriveComponents();
  return theme;
}

const GuiTheme& GuiTheme::dark() {
  // Built on first use, so another file's static initialiser can ask.
  static const GuiTheme theme = fromPalette(GUI_PALETTE_DARK, "dark");
  return theme;
}

const GuiTheme& GuiTheme::light() {
  static const GuiTheme theme = fromPalette(GUI_PALETTE_LIGHT, "light");
  return theme;
}

}  // namespace eng
