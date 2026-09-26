#include <catch2/catch_test_macros.hpp>
#include <game/ui/ui-actions.h>
#include <game/ui/ui-screen-json.h>
#include <string>

using namespace eng;
using namespace eng::game;

namespace {

/// A pause menu: a titled panel with a score, a health bar and two
/// buttons.
constexpr std::string_view PAUSE = R"({
  "schema": "simplish/ui_screen/1.0",
  "layer": "menu",
  "anchor": "center",
  "root": {
    "type": "panel", "direction": "column", "gap": 12,
    "padding": [20, 24], "width": 320, "fill": "#181c26e6", "radius": 8,
    "align": "center",
    "children": [
      {"type": "label", "text": "Paused", "id": "title"},
      {"type": "label", "text": "Score: {score}"},
      {"type": "bar", "value": "health", "max": "health_max", "height": 10},
      {"type": "button", "text": "Resume", "action": "resume", "id": "go"},
      {"type": "button", "text": "Quit", "action": "quit"}
    ]
  }
})";

/// Whether @p problems has one containing @p text.
bool mentions(const std::vector<std::string>& problems, std::string_view text) {
  for (const std::string& problem : problems) {
    if (problem.find(text) != std::string::npos) {
      return true;
    }
  }
  return false;
}

}  // namespace

TEST_CASE("a screen file reads into its nodes, styled") {
  const UiScreenRead read = parseUiScreen(PAUSE, "pause");

  REQUIRE(read.problems.empty());
  REQUIRE(read.screen.has_value());
  const UiNode& root = read.screen->root;
  CHECK(read.screen->id == "pause");
  CHECK(root.kind == UiNodeKind::PANEL);
  CHECK(root.style.width == 320.0F);
  CHECK(root.style.padding.left == 24.0F);
  CHECK(root.style.fill->a == 0xe6);
  REQUIRE(root.children.size() == 5);
  CHECK(root.children[2].kind == UiNodeKind::BAR);
  CHECK(root.children[3].action == "resume");
}

TEST_CASE("a screen file's mistakes are each named where they are") {
  const UiScreenRead read = parseUiScreen(R"({
    "layer": "popup",
    "root": {"type": "panel", "colour": "#fff", "children": [
      {"type": "buttn", "text": "Go"},
      {"type": "button", "text": "Go"},
      {"type": "label", "align": "middle", "text": "Hi"}]}})",
                                          "broken");

  REQUIRE(read.screen.has_value());
  CHECK(mentions(read.problems, "layer: should be menu or hud"));
  CHECK(mentions(read.problems, "root: unknown key 'colour'"));
  CHECK(mentions(read.problems, "root/children[0]: a node is an object"));
  CHECK(mentions(
      read.problems,
      "root/children[1]: a button, checkbox or toggle needs an 'action'"));
  CHECK(mentions(read.problems, "root/children[2]: 'align' should be"));
  CHECK(read.screen->root.children.size() == 2);
}

TEST_CASE("text that is no screen reads as nothing, and says why") {
  CHECK_FALSE(parseUiScreen("not json", "x").screen.has_value());
  const UiScreenRead rootless = parseUiScreen(R"({"layer": "hud"})", "x");
  CHECK_FALSE(rootless.screen.has_value());
  CHECK(mentions(rootless.problems, "a screen needs a root node"));
}

TEST_CASE("a project's actions are every button's, sorted, once each") {
  const UiScreen pause = *parseUiScreen(PAUSE, "pause").screen;
  const UiScreen dead = *parseUiScreen(R"({"root": {"type": "panel",
      "children": [{"type": "button", "text": "Again", "action": "retry"},
                   {"type": "button", "text": "Quit", "action": "quit"}]}})",
                                       "dead")
                             .screen;
  const UiScreen screens[] = {pause, dead};

  CHECK(uiActions(screens) ==
        std::vector<std::string>{"quit", "resume", "retry"});
}

namespace {

/// A raised half-width panel pushed to the middle, holding a heading and a
/// primary button.
constexpr std::string_view LOOK = R"({"root": {
    "type": "panel", "width": "50%", "max_width": 480, "elevation": "mid",
    "border": 1, "border_color": "#445566", "opacity": 0.5,
    "margin": [0, "auto"], "shrink": 0,
    "children": [
      {"type": "label", "text": "Hi", "role": "heading", "size": 30,
       "weight": 700, "wrap": true, "text_align": "center"},
      {"type": "button", "text": "Go", "action": "go", "variant": "primary"}
    ]}})";

}  // namespace

TEST_CASE("a node's box and sizes read in the theme's words") {
  const UiScreenRead read = parseUiScreen(LOOK, "look");

  REQUIRE(read.screen.has_value());
  CHECK(read.problems.empty());
  const UiNodeStyle& panel = read.screen->root.style;
  CHECK(panel.width_percent == 50.0F);
  CHECK(panel.width == -1.0F);
  CHECK(panel.max_width == 480.0F);
  CHECK(panel.elevation == GuiElevation::MID);
  CHECK(panel.border == 1.0F);
  CHECK(panel.opacity == 0.5F);
  CHECK((panel.margin_auto.left && panel.margin_auto.right));
  CHECK_FALSE(panel.margin_auto.top);
  CHECK(panel.shrink == 0.0F);
}

TEST_CASE("a node's text and variant read in the theme's words") {
  const UiScreenRead read = parseUiScreen(LOOK, "look");

  REQUIRE(read.screen.has_value());
  const UiNodeStyle& label = read.screen->root.children[0].style;
  CHECK(label.role == GuiTextRole::HEADING);
  CHECK(label.text_size == 30.0F);
  CHECK(label.weight == 700);
  CHECK(label.wrap == GuiTextWrap::WORD);
  CHECK(label.text_align == GuiLabelAlign::CENTER);
  CHECK(read.screen->root.children[1].style.variant ==
        GuiButtonVariant::PRIMARY);
}

TEST_CASE("a node binds its flags to values, and checkboxes and toggles "
          "choose actions") {
  const UiScreenRead read = parseUiScreen(R"({"root": {"type": "panel",
    "children": [
      {"type": "checkbox", "text": "Music", "action": "music",
       "checked": "music_on"},
      {"type": "toggle", "text": "Hard", "action": "hard",
       "disabled": "!can_choose"},
      {"type": "label", "text": "New!", "visible": "fresh",
       "selected": "picked"}]}})",
                                          "binds");

  REQUIRE(read.screen.has_value());
  CHECK(read.problems.empty());
  const auto& kids = read.screen->root.children;
  CHECK(kids[0].kind == UiNodeKind::CHECKBOX);
  CHECK(kids[0].bind.checked == "music_on");
  CHECK(kids[1].kind == UiNodeKind::TOGGLE);
  CHECK(kids[1].bind.disabled == "!can_choose");
  CHECK(kids[2].bind.visible == "fresh");
  CHECK(kids[2].bind.selected == "picked");
  const UiScreen screens[] = {*read.screen};
  CHECK(uiActions(screens) == std::vector<std::string>{"hard", "music"});
}

TEST_CASE("a look that does not read is named, and keeps its default") {
  const UiScreenRead read = parseUiScreen(R"({"root": {"type": "panel",
    "width": "wide", "opacity": 2, "margin": [1, "far"],
    "children": [
      {"type": "button", "text": "Go", "action": "go", "variant": "big"},
      {"type": "label", "text": "x", "weight": 50, "checked": "on"}]}})",
                                          "bad");

  REQUIRE(read.screen.has_value());
  CHECK(mentions(read.problems, "root: 'width' should be a number of pixels"));
  CHECK(mentions(read.problems, "root: 'opacity' should be from 0 to 1"));
  CHECK(mentions(read.problems, "root: 'margin' should be"));
  CHECK(mentions(read.problems,
                 "root/children[0]: 'variant' should be one of neutral, "
                 "primary, danger, ghost"));
  CHECK(mentions(read.problems,
                 "root/children[1]: 'weight' should be from 100 to 900"));
  CHECK(mentions(read.problems,
                 "root/children[1]: only a checkbox or toggle is 'checked'"));
  CHECK(read.screen->root.style.opacity == 1.0F);
  CHECK(read.screen->root.style.width_percent == -1.0F);
}
