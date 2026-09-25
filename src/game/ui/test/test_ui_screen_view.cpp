#include <catch2/catch_test_macros.hpp>
#include <engine/gui/gui-button.h>
#include <engine/gui/gui-label.h>
#include <engine/gui/gui-mouse-event.h>
#include <engine/gui/gui-widget-tree.h>
#include <game/ui/ui-screen-json.h>
#include <game/ui/ui-screen-view.h>
#include <string>
#include <vector>

using namespace eng;
using namespace eng::game;

namespace {

/// A column of a score label, a health bar and two buttons, centred.
constexpr std::string_view SCREEN = R"json({
  "root": {"type": "panel", "width": 300, "gap": 8, "padding": 10,
    "children": [
      {"type": "label", "text": "Score: {score}", "id": "score"},
      {"type": "bar", "value": "health", "max": "10", "height": 10},
      {"type": "button", "text": "Again ({tries})", "action": "retry"},
      {"type": "button", "text": "Quit", "action": "quit", "id": "quit"}]}})json";

/// A tree with a root the size of an 800 by 600 view, and a screen built
/// in it, laid out, whose presses are written down.
struct Built {
  GuiWidgetTree tree;
  GuiWidgetId root =
      tree.createWidget(GuiWidgetType::PANEL, GUI_WIDGET_ID_INVALID);
  std::vector<std::string> pressed;
  UiScreenView view{
      *parseUiScreen(SCREEN, "over").screen,
      [this](std::string_view action) { pressed.emplace_back(action); }};

  Built() {
    view.build(tree, root);
    layout();
  }
  void layout() { tree.computeLayout(makeRect(0.0F, 0.0F, 800.0F, 600.0F)); }
  const GuiWidget& byId(std::string_view id) { return *tree.findById(id); }
};

}  // namespace

TEST_CASE("a built screen shows its values, and says when to lay out again") {
  Built built;

  CHECK(built.view.apply(built.tree, {{"score", "12"}, {"tries", "2"}}));
  CHECK(dynamic_cast<const GuiLabel&>(built.byId("score")).text == "Score: 12");
  CHECK(built.view.buttons(built.tree)[0].text == "Again (2)");
  CHECK_FALSE(built.view.apply(built.tree, {{"score", "12"}, {"tries", "2"}}));
}

TEST_CASE("a built screen's root sits in the middle, laid out by flexbox") {
  Built built;

  const Rect quit = built.byId("quit").rect;
  CHECK(quit.w == 280.0F);  // the column's 300, less its padding
  CHECK(quit.x == 260.0F);  // (800 - 300) / 2 + 10
  CHECK(built.view.buttons(built.tree)[1].rect.y == quit.y);
}

TEST_CASE("a built screen's bar fills by its value's share") {
  Built built;
  (void)built.view.apply(built.tree, {{"health", "5"}});
  built.layout();

  const GuiWidget& track = *built.tree.findWidget(
      built.tree
          .findWidget(
              built.tree.findWidget(built.view.overlay())->children.front())
          ->children[1]);
  const GuiWidget& fill = *built.tree.findWidget(track.children[0]);
  CHECK(fill.rect.w == track.rect.w / 2.0F);
}

TEST_CASE("pressing a built screen's button reports its action") {
  Built built;

  built.tree.setFocus(built.view.firstButton());
  CHECK(built.tree.routeNav(GuiNavCommand::CONFIRM));
  const Rect quit = built.byId("quit").rect;
  (void)built.tree.dispatchClick(quit.x + 4.0F, quit.y + 4.0F);

  CHECK(built.pressed == std::vector<std::string>{"retry", "quit"});
}

TEST_CASE("a destroyed screen leaves nothing in the tree") {
  Built built;
  const size_t before = built.tree.widget_nodes.size();

  built.view.destroy(built.tree);

  CHECK(built.tree.widget_nodes.size() < before);
  CHECK(built.tree.widget_nodes.size() == 1);
}
