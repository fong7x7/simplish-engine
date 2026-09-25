#include <catch2/catch_test_macros.hpp>
#include <engine/gui/gui-theme-json.h>

using namespace eng;

TEST_CASE("a theme file lays its palette and scales over its base") {
  std::string error;
  const auto theme = parseGuiTheme(R"({
    "name": "Ember", "base": "light",
    "palette": { "primary": "#e8703a", "scrim": "#00000080" },
    "spacing": [0, 3, 6], "radii": [0, 4], "transition_ms": 200 })",
                                   error);
  REQUIRE(theme.has_value());
  CHECK(theme->name == "Ember");
  CHECK(theme->palette.primary.pack() == GuiColor{232, 112, 58}.pack());
  CHECK(theme->palette.scrim.a == 0x80);
  CHECK(theme->palette.background.pack() ==
        GuiTheme::light().palette.background.pack());
  CHECK(theme->space(GuiSpace::XS) == 6.0f);
  CHECK(theme->space(GuiSpace::MD) == 12.0f);
  CHECK(theme->radius(GuiRadius::SM) == 4.0f);
  CHECK(theme->transition_seconds == 0.2f);
  // Components follow the new palette.
  CHECK(theme->button(GuiButtonVariant::PRIMARY)
            .of(GuiWidgetState::NORMAL)
            .fill.pack() == GuiColor{232, 112, 58}.pack());
}

TEST_CASE("an empty theme file is the dark theme") {
  std::string error;
  const auto theme = parseGuiTheme("{}", error);
  REQUIRE(theme.has_value());
  CHECK(theme->palette.primary.pack() ==
        GuiTheme::dark().palette.primary.pack());
}

TEST_CASE("a bad theme file says what is wrong") {
  std::string error;
  CHECK_FALSE(parseGuiTheme("not json", error));
  CHECK(error == "not a JSON object");
  CHECK_FALSE(parseGuiTheme(R"({"palette": {"primray": "#fff000"}})", error));
  CHECK(error == "palette.primray: not a palette colour");
  CHECK_FALSE(parseGuiTheme(R"({"palette": {"primary": "orange"}})", error));
  CHECK(error == "palette.primary: not a colour, #rrggbb or #rrggbbaa");
  CHECK_FALSE(parseGuiTheme(R"({"spacing": [1, "x"]})", error));
  CHECK(error == "spacing: not an array of up to 8 numbers");
  CHECK_FALSE(parseGuiTheme(R"({"base": "sepia"})", error));
}

TEST_CASE("colours are read as #rrggbb or #rrggbbaa") {
  CHECK(parseGuiColor("#102030")->pack() == GuiColor{16, 32, 48}.pack());
  CHECK(parseGuiColor("#10203040")->a == 64);
  CHECK_FALSE(parseGuiColor("#12345"));
  CHECK_FALSE(parseGuiColor("102030"));
  CHECK_FALSE(parseGuiColor("#10zz30"));
}
