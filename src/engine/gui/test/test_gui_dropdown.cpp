#include <catch2/catch_test_macros.hpp>
#include <engine/gui/gui-dropdown.h>

using namespace eng;

namespace {

/// A three-row menu: one command, a separator, one disabled command.
/// Row height and width are set explicitly because `hitTestItem` reads the
/// per-instance style rather than a shared one.
GuiDropdown makeMenu(int& selected) {
  GuiDropdown menu;
  menu.style.width = 100;
  menu.style.item_height = 20;
  menu.rect = {0.0f, 0.0f, 100.0f, 60.0f};
  menu.items.push_back({.label = "Open",
                        .on_select = [&selected]() { selected = 0; },
                        .shortcut = "Ctrl+O"});
  menu.items.push_back({.separator = true});
  menu.items.push_back({.label = "Save",
                        .on_select = [&selected]() { selected = 2; },
                        .enabled = false});
  return menu;
}

}  // namespace

TEST_CASE("hitTestItem maps a y offset to its row") {
  int selected = -1;
  const GuiDropdown menu = makeMenu(selected);

  REQUIRE(menu.hitTestItem(50.0f, 0.0f) == 0);
  REQUIRE(menu.hitTestItem(50.0f, 19.0f) == 0);
  REQUIRE(menu.hitTestItem(50.0f, 20.0f) == 1);
  REQUIRE(menu.hitTestItem(50.0f, 45.0f) == 2);
}

TEST_CASE("hitTestItem rejects points outside the menu") {
  int selected = -1;
  const GuiDropdown menu = makeMenu(selected);

  REQUIRE(menu.hitTestItem(-1.0f, 10.0f) == -1);
  REQUIRE(menu.hitTestItem(100.0f, 10.0f) == -1);
  REQUIRE(menu.hitTestItem(50.0f, -1.0f) == -1);
  REQUIRE(menu.hitTestItem(50.0f, 60.0f) == -1);
}

TEST_CASE("an invisible menu is never hit") {
  int selected = -1;
  GuiDropdown menu = makeMenu(selected);
  menu.visible = false;

  REQUIRE(menu.hitTestItem(50.0f, 10.0f) == -1);
}

TEST_CASE("selecting an enabled row fires its callback") {
  int selected = -1;
  GuiDropdown menu = makeMenu(selected);

  menu.selectItem(0);
  REQUIRE(selected == 0);
}

TEST_CASE("separators and disabled rows are not selectable") {
  int selected = -1;
  GuiDropdown menu = makeMenu(selected);

  menu.selectItem(1);
  menu.selectItem(2);
  REQUIRE(selected == -1);
}

TEST_CASE("selecting out of range does nothing") {
  int selected = -1;
  GuiDropdown menu = makeMenu(selected);

  menu.selectItem(-1);
  menu.selectItem(3);
  REQUIRE(selected == -1);
}

TEST_CASE("a separator still occupies a row, so indices stay a division") {
  int selected = -1;
  const GuiDropdown menu = makeMenu(selected);

  // "Save" is the third item and sits in the third row, even though the row
  // above it draws a divider rather than a label.
  REQUIRE(menu.items[2].label == "Save");
  REQUIRE(menu.hitTestItem(50.0f, 40.0f) == 2);
}
