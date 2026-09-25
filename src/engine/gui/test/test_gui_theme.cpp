#include <catch2/catch_test_macros.hpp>
#include <engine/gui/gui-theme-constants.h>
#include <engine/gui/gui-theme.h>

using namespace eng;

namespace {

/// Whether @p a and @p b are the same colour.
bool same(const GuiColor& a, const GuiColor& b) {
  return a.pack() == b.pack();
}

}  // namespace

TEST_CASE(
    "the dark theme's neutral button lights with the accent when chosen") {
  const GuiStateStyles& neutral =
      GuiTheme::dark().button(GuiButtonVariant::NEUTRAL);
  CHECK(same(neutral.of(GuiWidgetState::NORMAL).fill, THEME_BTN));
  CHECK(same(neutral.of(GuiWidgetState::HOVER).fill, THEME_BTN_HOVER));
  CHECK(same(neutral.of(GuiWidgetState::SELECTED).fill, THEME_ACCENT));
  CHECK(neutral.of(GuiWidgetState::NORMAL).radius == THEME_BTN_RADIUS);
}

TEST_CASE("a disabled button is dimmed and its text greyed") {
  const GuiTheme& theme = GuiTheme::dark();
  const GuiStateStyle& off =
      theme.button(GuiButtonVariant::PRIMARY).of(GuiWidgetState::DISABLED);
  CHECK(off.fill.a < theme.palette.primary.a);
  CHECK(same(off.text, theme.palette.text_disabled));
}

TEST_CASE("a ghost button has no box until hovered") {
  const GuiStateStyles& ghost =
      GuiTheme::dark().button(GuiButtonVariant::GHOST);
  CHECK(ghost.of(GuiWidgetState::NORMAL).fill.a == 0);
  CHECK(ghost.of(GuiWidgetState::HOVER).fill.a > 0);
}

TEST_CASE("a field takes the accent border while focused") {
  const GuiTheme& theme = GuiTheme::light();
  CHECK(same(theme.field.of(GuiWidgetState::FOCUSED).border,
             theme.palette.primary));
  CHECK(theme.field.of(GuiWidgetState::NORMAL).border_width == 1.0f);
}

TEST_CASE("a card is raised, and lifts further under the pointer") {
  const GuiTheme& theme = GuiTheme::dark();
  CHECK(theme.card.of(GuiWidgetState::NORMAL).elevation == GuiElevation::LOW);
  CHECK(theme.card.of(GuiWidgetState::HOVER).elevation == GuiElevation::MID);
  CHECK(theme.shadow(GuiElevation::MID).blur >
        theme.shadow(GuiElevation::LOW).blur);
  CHECK(theme.shadow(GuiElevation::NONE).color.a == 0);
}

TEST_CASE("scales are looked up by step") {
  const GuiTheme& theme = GuiTheme::dark();
  CHECK(theme.space(GuiSpace::NONE) == 0.0f);
  CHECK(theme.space(GuiSpace::MD) == 12.0f);
  CHECK(theme.radius(GuiRadius::LG) == 8.0f);
  CHECK(theme.textSize(GuiTextSize::MD) == 14.0f);
}

TEST_CASE("changing the palette and deriving restyles every component") {
  GuiTheme theme = GuiTheme::dark();
  theme.palette.primary = {200, 80, 20};
  theme.deriveComponents();
  CHECK(same(
      theme.button(GuiButtonVariant::PRIMARY).of(GuiWidgetState::NORMAL).fill,
      {200, 80, 20}));
  CHECK(same(theme.field.of(GuiWidgetState::FOCUSED).border, {200, 80, 20}));
}
