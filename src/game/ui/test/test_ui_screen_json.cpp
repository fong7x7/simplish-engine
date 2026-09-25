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
  CHECK(
      mentions(read.problems, "root/children[1]: a button needs an 'action'"));
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
