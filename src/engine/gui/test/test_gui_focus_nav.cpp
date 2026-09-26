#include <catch2/catch_test_macros.hpp>
#include <engine/gui/gui-button.h>
#include <engine/gui/gui-dropdown.h>
#include <engine/gui/gui-slider.h>
#include <engine/gui/gui-text-input.h>
#include <engine/gui/gui-widget-tree.h>
#include <memory>
#include <string>
#include <vector>

using namespace eng;

namespace {

constexpr Rect WINDOW{0.0f, 0.0f, 1000.0f, 800.0f};

/// A window-sized root to hang a menu's widgets on.
struct MenuFixture {
  GuiWidgetTree tree;
  GuiWidgetId root{
      tree.createWidget(GuiWidgetType::PANEL, GUI_WIDGET_ID_INVALID)};

  MenuFixture() { tree.findWidget(root)->rect = WINDOW; }

  /// A button under @p parent at @p rect, counting its presses into
  /// @p presses when given.
  GuiWidgetId button(GuiWidgetId parent, const Rect& rect,
                     int* presses = nullptr) {
    const GuiWidgetId id = tree.createWidget(GuiWidgetType::BUTTON, parent);
    GuiWidget* widget = tree.findWidget(id);
    widget->rect = rect;
    if (presses != nullptr) {
      widget->onClick([presses](const GuiMouseEvent&) { ++*presses; });
    }
    return id;
  }

  /// A panel under @p parent at @p rect, which does not take focus.
  GuiWidgetId panel(GuiWidgetId parent, const Rect& rect) {
    const GuiWidgetId id = tree.createWidget(GuiWidgetType::PANEL, parent);
    tree.findWidget(id)->rect = rect;
    return id;
  }

  /// @p widget, inserted under the root at @p rect.
  template <typename T> T& insert(std::unique_ptr<T> widget, const Rect& rect) {
    T& ref = *widget;
    ref.rect = rect;
    (void)tree.insertExternalWidget(std::move(widget), root);
    return ref;
  }
};

/// A 2x2 grid of buttons: top-left, top-right, bottom-left, bottom-right.
struct Grid {
  GuiWidgetId tl;
  GuiWidgetId tr;
  GuiWidgetId bl;
  GuiWidgetId br;
};

/// Rows Easy, a separator, Hard, and a disabled Off, each recording its
/// index into @p picked when chosen.
void fillDifficulty(GuiDropdown& menu, int& picked) {
  menu.items.push_back({.label = "Easy", .on_select = [&] { picked = 0; }});
  menu.items.push_back({.separator = true});
  menu.items.push_back({.label = "Hard", .on_select = [&] { picked = 2; }});
  menu.items.push_back({.label = "Off", .enabled = false});
}

Grid makeGrid(MenuFixture& fx) {
  return {fx.button(fx.root, {100, 100, 100, 40}),
          fx.button(fx.root, {300, 100, 100, 40}),
          fx.button(fx.root, {100, 200, 100, 40}),
          fx.button(fx.root, {300, 200, 100, 40})};
}

}  // namespace

TEST_CASE("the first command focuses the first focusable widget") {
  MenuFixture fx;
  (void)fx.panel(fx.root, {0, 0, 50, 50});
  const Grid grid = makeGrid(fx);
  REQUIRE(fx.tree.focused_id == GUI_WIDGET_ID_INVALID);

  REQUIRE(fx.tree.routeNav(GuiNavCommand::DOWN));
  REQUIRE(fx.tree.focused_id == grid.tl);
  REQUIRE(fx.tree.focus_visibility == GuiFocusVisibility::SHOWN);
}

TEST_CASE("directions move focus along the rows and columns of a grid") {
  MenuFixture fx;
  const Grid grid = makeGrid(fx);
  fx.tree.setFocus(grid.tl);

  REQUIRE(fx.tree.routeNav(GuiNavCommand::RIGHT));
  REQUIRE(fx.tree.focused_id == grid.tr);
  REQUIRE(fx.tree.routeNav(GuiNavCommand::DOWN));
  REQUIRE(fx.tree.focused_id == grid.br);
  REQUIRE(fx.tree.routeNav(GuiNavCommand::LEFT));
  REQUIRE(fx.tree.focused_id == grid.bl);
  REQUIRE(fx.tree.routeNav(GuiNavCommand::UP));
  REQUIRE(fx.tree.focused_id == grid.tl);
}

TEST_CASE("with nothing that way, focus stays and the command is unused") {
  MenuFixture fx;
  const Grid grid = makeGrid(fx);
  fx.tree.setFocus(grid.tl);

  REQUIRE_FALSE(fx.tree.routeNav(GuiNavCommand::UP));
  REQUIRE_FALSE(fx.tree.routeNav(GuiNavCommand::LEFT));
  REQUIRE(fx.tree.focused_id == grid.tl);
}

TEST_CASE("a widget straight ahead beats a nearer one off to the side") {
  MenuFixture fx;
  const GuiWidgetId from = fx.button(fx.root, {100, 100, 100, 40});
  const GuiWidgetId ahead = fx.button(fx.root, {100, 300, 100, 40});
  (void)fx.button(fx.root, {400, 160, 100, 40});
  fx.tree.setFocus(from);

  REQUIRE(fx.tree.routeNav(GuiNavCommand::DOWN));
  REQUIRE(fx.tree.focused_id == ahead);
}

TEST_CASE("next and previous step through focus order and wrap") {
  MenuFixture fx;
  const Grid grid = makeGrid(fx);
  fx.tree.setFocus(grid.br);

  REQUIRE(fx.tree.routeNav(GuiNavCommand::NEXT));
  REQUIRE(fx.tree.focused_id == grid.tl);
  REQUIRE(fx.tree.routeNav(GuiNavCommand::PREVIOUS));
  REQUIRE(fx.tree.focused_id == grid.br);
}

TEST_CASE("confirm presses the focused button") {
  MenuFixture fx;
  int presses = 0;
  const GuiWidgetId play = fx.button(fx.root, {100, 100, 100, 40}, &presses);
  fx.tree.setFocus(play);

  REQUIRE(fx.tree.routeNav(GuiNavCommand::CONFIRM));
  REQUIRE(presses == 1);
}

TEST_CASE("cancel nothing takes is the caller's, to close the menu") {
  MenuFixture fx;
  const Grid grid = makeGrid(fx);
  fx.tree.setFocus(grid.tl);
  REQUIRE_FALSE(fx.tree.routeNav(GuiNavCommand::CANCEL));
  REQUIRE(fx.tree.focused_id == grid.tl);
}

TEST_CASE("left and right step a slider and report the value") {
  MenuFixture fx;
  auto& volume = fx.insert(std::make_unique<GuiSlider>(), {100, 100, 200, 20});
  float reported = -1.0f;
  volume.value = 0.5f;
  volume.on_change = [&reported](float v) {
    reported = v;
  };
  fx.tree.setFocus(volume.widget_id);

  REQUIRE(fx.tree.routeNav(GuiNavCommand::RIGHT));
  REQUIRE(reported == 0.55f);
  (void)fx.tree.routeNav(GuiNavCommand::LEFT);
  (void)fx.tree.routeNav(GuiNavCommand::LEFT);
  REQUIRE(reported < 0.5f);
  REQUIRE(fx.tree.focused_id == volume.widget_id);
}

TEST_CASE("up and down leave a slider") {
  MenuFixture fx;
  auto& volume = fx.insert(std::make_unique<GuiSlider>(), {100, 100, 200, 20});
  const GuiWidgetId below = fx.button(fx.root, {100, 200, 100, 40});
  fx.tree.setFocus(volume.widget_id);
  REQUIRE(fx.tree.routeNav(GuiNavCommand::DOWN));
  REQUIRE(fx.tree.focused_id == below);
}

TEST_CASE("a slider stops at its ends") {
  MenuFixture fx;
  auto& slider = fx.insert(std::make_unique<GuiSlider>(), {0, 0, 100, 20});
  slider.value = 0.98f;
  fx.tree.setFocus(slider.widget_id);
  REQUIRE(fx.tree.routeNav(GuiNavCommand::RIGHT));
  REQUIRE(fx.tree.routeNav(GuiNavCommand::RIGHT));
  REQUIRE(slider.value == 1.0f);
}

TEST_CASE("an open dropdown walks its selectable rows and confirms one") {
  MenuFixture fx;
  auto& menu = fx.insert(std::make_unique<GuiDropdown>(), {0, 0, 100, 80});
  int picked = -1;
  fillDifficulty(menu, picked);
  fx.tree.setFocus(menu.widget_id);

  REQUIRE(fx.tree.routeNav(GuiNavCommand::DOWN));
  REQUIRE(menu.hovered_item == 0);
  REQUIRE(fx.tree.routeNav(GuiNavCommand::DOWN));
  REQUIRE(menu.hovered_item == 2);
  // The disabled last row is skipped, and the end keeps focus.
  REQUIRE(fx.tree.routeNav(GuiNavCommand::DOWN));
  REQUIRE(menu.hovered_item == 2);
  REQUIRE(fx.tree.focused_id == menu.widget_id);

  REQUIRE(fx.tree.routeNav(GuiNavCommand::CONFIRM));
  REQUIRE(picked == 2);
}

TEST_CASE("confirm starts typing in a text field, and cancel stops") {
  MenuFixture fx;
  auto& name = fx.insert(std::make_unique<GuiTextInput>(), {100, 100, 200, 30});
  fx.tree.setFocus(name.widget_id);
  REQUIRE_FALSE(fx.tree.hasFocusedInput());

  REQUIRE(fx.tree.routeNav(GuiNavCommand::CONFIRM));
  REQUIRE(fx.tree.hasFocusedInput());
  REQUIRE(name.focus == GuiTextInputFocus::FOCUSED);

  REQUIRE(fx.tree.routeNav(GuiNavCommand::CANCEL));
  REQUIRE_FALSE(fx.tree.hasFocusedInput());
  REQUIRE(fx.tree.focused_id == name.widget_id);
}

TEST_CASE("moving off a text field stops typing in it") {
  MenuFixture fx;
  auto& name = fx.insert(std::make_unique<GuiTextInput>(), {100, 100, 200, 30});
  const GuiWidgetId ok = fx.button(fx.root, {100, 200, 100, 40});
  fx.tree.setFocus(name.widget_id);
  REQUIRE(fx.tree.routeNav(GuiNavCommand::CONFIRM));

  REQUIRE(fx.tree.routeNav(GuiNavCommand::DOWN));
  REQUIRE(fx.tree.focused_id == ok);
  REQUIRE_FALSE(fx.tree.hasFocusedInput());
}

TEST_CASE("a focus scope keeps navigation inside an open dialog") {
  MenuFixture fx;
  const GuiWidgetId behind = fx.button(fx.root, {100, 100, 100, 40});
  const GuiWidgetId dialog = fx.panel(fx.root, {400, 300, 300, 200});
  const GuiWidgetId yes = fx.button(dialog, {420, 400, 100, 40});
  const GuiWidgetId no = fx.button(dialog, {560, 400, 100, 40});
  fx.tree.setFocus(behind);

  fx.tree.setFocusScope(dialog);
  REQUIRE(fx.tree.focused_id == yes);
  REQUIRE(fx.tree.routeNav(GuiNavCommand::RIGHT));
  REQUIRE(fx.tree.focused_id == no);
  REQUIRE(fx.tree.routeNav(GuiNavCommand::NEXT));
  REQUIRE(fx.tree.focused_id == yes);
  // The button behind is up and to the left, but out of scope.
  REQUIRE_FALSE(fx.tree.routeNav(GuiNavCommand::UP));

  fx.tree.setFocusScope(GUI_WIDGET_ID_INVALID);
  REQUIRE(fx.tree.routeNav(GuiNavCommand::UP));
  REQUIRE(fx.tree.focused_id == behind);
}

TEST_CASE("a hidden panel hides every widget in it from navigation") {
  MenuFixture fx;
  const GuiWidgetId shown = fx.button(fx.root, {100, 100, 100, 40});
  const GuiWidgetId closed = fx.panel(fx.root, {100, 200, 300, 200});
  (void)fx.button(closed, {100, 220, 100, 40});
  fx.tree.findWidget(closed)->visible = false;
  fx.tree.setFocus(shown);

  REQUIRE_FALSE(fx.tree.routeNav(GuiNavCommand::DOWN));
  REQUIRE(fx.tree.focused_id == shown);
}

TEST_CASE("focus on a widget that is hidden starts over at the first") {
  MenuFixture fx;
  const Grid grid = makeGrid(fx);
  fx.tree.setFocus(grid.br);
  fx.tree.findWidget(grid.br)->visible = false;

  REQUIRE(fx.tree.routeNav(GuiNavCommand::RIGHT));
  REQUIRE(fx.tree.focused_id == grid.tl);
}

TEST_CASE("the pointer hides the ring and a click moves focus") {
  MenuFixture fx;
  const Grid grid = makeGrid(fx);
  REQUIRE(fx.tree.routeNav(GuiNavCommand::DOWN));
  REQUIRE(fx.tree.focus_visibility == GuiFocusVisibility::SHOWN);

  fx.tree.dispatchMouseMove({.x = 5.0f, .y = 5.0f});
  REQUIRE(fx.tree.focus_visibility == GuiFocusVisibility::HIDDEN);

  (void)fx.tree.dispatchMouseDown(
      {.type = GuiMouseEventType::BUTTON_DOWN, .x = 350.0f, .y = 220.0f});
  REQUIRE(fx.tree.focused_id == grid.br);
}

TEST_CASE("a custom widget joins navigation by taking focus") {
  MenuFixture fx;
  const GuiWidgetId card = fx.panel(fx.root, {100, 100, 100, 100});
  REQUIRE_FALSE(fx.tree.routeNav(GuiNavCommand::DOWN));
  fx.tree.findWidget(card)->tree_focusable = true;
  REQUIRE(fx.tree.routeNav(GuiNavCommand::DOWN));
  REQUIRE(fx.tree.focused_id == card);
}

TEST_CASE("widgets in the overlay layer take part in navigation") {
  MenuFixture fx;
  const GuiWidgetId tree_button = fx.button(fx.root, {100, 100, 100, 40});
  const GuiWidgetId popover =
      fx.button(fx.tree.overlayLayer(), {100, 300, 100, 40});
  fx.tree.setFocus(tree_button);

  REQUIRE(fx.tree.routeNav(GuiNavCommand::DOWN));
  REQUIRE(fx.tree.focused_id == popover);
  REQUIRE(fx.tree.routeNav(GuiNavCommand::UP));
  REQUIRE(fx.tree.focused_id == tree_button);
}

TEST_CASE("confirm presses a focused widget in the overlay layer") {
  MenuFixture fx;
  const GuiWidgetId popover =
      fx.button(fx.tree.overlayLayer(), {100, 300, 100, 40});
  int presses = 0;
  fx.tree.findWidget(popover)->onClick(
      [&presses](const GuiMouseEvent&) { ++presses; });
  fx.tree.setFocus(popover);
  REQUIRE(fx.tree.routeNav(GuiNavCommand::CONFIRM));
  REQUIRE(presses == 1);
}

TEST_CASE("an overlay destroyed while focused drops focus") {
  MenuFixture fx;
  const GuiWidgetId popover =
      fx.button(fx.tree.overlayLayer(), {100, 300, 100, 40});
  fx.tree.setFocus(popover);
  REQUIRE(fx.tree.focused_id == popover);
  fx.tree.destroyWidget(popover);
  REQUIRE(fx.tree.focusedWidget() == nullptr);
}

TEST_CASE("a focus scope leaves the rest of the tree out, overlays too") {
  MenuFixture fx;
  const GuiWidgetId dialog = fx.panel(fx.root, {0, 0, 400, 200});
  const GuiWidgetId ok = fx.button(dialog, {100, 100, 100, 40});
  (void)fx.button(fx.tree.overlayLayer(), {100, 300, 100, 40});
  fx.tree.setFocusScope(dialog);

  REQUIRE(fx.tree.focused_id == ok);
  REQUIRE_FALSE(fx.tree.routeNav(GuiNavCommand::DOWN));
}

TEST_CASE("a widget hears when it gains and loses focus") {
  MenuFixture fx;
  const GuiWidgetId first = fx.button(fx.root, {100, 100, 100, 40});
  const GuiWidgetId second = fx.button(fx.root, {100, 200, 100, 40});
  std::vector<std::string> heard;
  fx.tree.findWidget(first)->onFocusChange([&heard](GuiFocusChange change) {
    heard.emplace_back(change == GuiFocusChange::GAINED ? "first+" : "first-");
  });
  fx.tree.findWidget(second)->onFocusChange([&heard](GuiFocusChange change) {
    heard.emplace_back(change == GuiFocusChange::GAINED ? "second+"
                                                        : "second-");
  });

  fx.tree.setFocus(first);
  fx.tree.setFocus(first);
  REQUIRE(fx.tree.routeNav(GuiNavCommand::DOWN));
  REQUIRE(heard == std::vector<std::string>{"first+", "first-", "second+"});
}
