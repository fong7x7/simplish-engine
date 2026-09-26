#include <catch2/catch_test_macros.hpp>
#include <engine/gui/gui-button.h>
#include <engine/gui/gui-checkbox.h>
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

namespace {

/// A settings card: a heading shown only while there is news, a music
/// checkbox showing its value, a toggle disabled until it may be chosen,
/// a half-width primary button pushed right, and the screens' theme.
constexpr std::string_view SETTINGS = R"json({
  "root": {"type": "panel", "width": 400, "padding": 10, "gap": 6,
    "elevation": "low",
    "children": [
      {"type": "label", "text": "News", "id": "news", "role": "heading",
       "visible": "news"},
      {"type": "checkbox", "text": "Music", "action": "music", "id": "music",
       "checked": "music_on"},
      {"type": "toggle", "text": "Hard", "action": "hard", "id": "hard",
       "disabled": "!can_choose"},
      {"type": "button", "text": "Done", "action": "done", "id": "done",
       "variant": "primary", "width": "50%", "margin": [0, 0, 0, "auto"],
       "align_self": "start"}]}})json";

/// The settings card built in the light theme, laid out in 800 by 600.
struct Settings {
  GuiWidgetTree tree;
  GuiWidgetId root =
      tree.createWidget(GuiWidgetType::PANEL, GUI_WIDGET_ID_INVALID);
  std::vector<std::string> pressed;
  std::shared_ptr<const GuiTheme> theme =
      std::make_shared<GuiTheme>(GuiTheme::light());
  UiScreenView view{
      *parseUiScreen(SETTINGS, "settings").screen,
      [this](std::string_view action) { pressed.emplace_back(action); }, theme};

  Settings() {
    view.build(tree, root);
    layout();
  }
  void layout() { tree.computeLayout(makeRect(0.0F, 0.0F, 800.0F, 600.0F)); }
  GuiWidget& byId(std::string_view id) { return *tree.findById(id); }
  bool apply(const UiValues& values) {
    const bool moved = view.apply(tree, values);
    layout();
    return moved;
  }
};

}  // namespace

TEST_CASE("a built screen's bound flags follow its values") {
  Settings s;
  CHECK_FALSE(s.byId("news").visible);
  CHECK(s.byId("hard").disabled);

  CHECK(s.apply({{"news", "1"}, {"can_choose", "true"}}));
  CHECK(s.byId("news").visible);
  CHECK_FALSE(s.byId("hard").disabled);
  CHECK_FALSE(s.apply({{"news", "1"}, {"can_choose", "true"}}));
}

TEST_CASE("a hidden node takes no room") {
  Settings s;
  const float hidden = s.byId("music").rect.y;
  (void)s.apply({{"news", "1"}});
  CHECK(s.byId("music").rect.y > hidden);
}

TEST_CASE("a bound checkbox shows its value, and a press only asks") {
  Settings s;
  auto& music = dynamic_cast<GuiCheckbox&>(s.byId("music"));
  (void)s.apply({{"music_on", "1"}});
  REQUIRE(music.state == GuiCheckState::CHECKED);

  const Rect at = music.rect;
  (void)s.tree.dispatchClick(at.x + 4.0F, at.y + 4.0F);

  CHECK(s.pressed == std::vector<std::string>{"music"});
  CHECK(music.state == GuiCheckState::CHECKED);  // until the logic says
  (void)s.apply({{"music_on", "0"}});
  CHECK(music.state == GuiCheckState::UNCHECKED);
}

TEST_CASE("a built screen is drawn in its theme, sized by percent, and "
          "pushed by an auto margin") {
  Settings s;
  CHECK(s.tree.themeAt(s.byId("done").widget_id) == s.theme.get());
  const auto& done = dynamic_cast<const GuiButton&>(s.byId("done"));
  CHECK(done.state_styles->of(GuiWidgetState::NORMAL).fill.pack() ==
        s.theme->palette.primary.pack());
  CHECK(done.rect.w == 190.0F);  // half the card's 380 of content
  CHECK(done.rect.x + done.rect.w == 800.0F / 2.0F + 200.0F - 10.0F);
}

TEST_CASE("a built screen lists its named nodes, with their flags") {
  Settings s;
  const std::vector<UiNodeInfo> nodes = s.view.nodes(s.tree);
  REQUIRE(nodes.size() == 4);
  CHECK(nodes[0].id == "news");
  CHECK_FALSE(nodes[0].visible);
  CHECK(nodes[2].kind == UiNodeKind::TOGGLE);
  CHECK(nodes[2].disabled);
  CHECK(nodes[3].rect.w == 190.0F);
  CHECK(s.view.buttons(s.tree).size() == 3);
}

TEST_CASE("a menu pops up, and its nodes glide") {
  Settings s;
  const GuiWidget& card =
      *s.tree.findWidget(s.tree.findWidget(s.view.overlay())->children.front());
  CHECK(card.render_scale == GUI_PRESENCE_POP.scale);
  CHECK(s.byId("done").layout_glide > 0.0F);
  s.tree.updateAll({}, 1.0F);
  CHECK(card.render_scale == 1.0F);
  CHECK(card.opacity == 1.0F);
}

TEST_CASE("a HUD only fades in, and its nodes keep still") {
  GuiWidgetTree tree;
  const GuiWidgetId root =
      tree.createWidget(GuiWidgetType::PANEL, GUI_WIDGET_ID_INVALID);
  UiScreenView hud(*parseUiScreen(R"({"layer": "hud", "root":
      {"type": "label", "text": "{score}", "id": "score"}})",
                                  "hud")
                        .screen,
                   [](std::string_view) {});
  (void)hud.build(tree, root);
  CHECK(tree.findById("score")->layout_glide == 0.0F);
  CHECK(tree.findById("score")->render_scale == 1.0F);
}
