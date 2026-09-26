#include <catch2/catch_test_macros.hpp>
#include <engine/gui/gui-checkbox.h>
#include <engine/gui/gui-draw-context.h>
#include <engine/gui/gui-radio-group.h>
#include <engine/gui/gui-tabs.h>
#include <engine/gui/gui-toggle.h>

using namespace eng;

TEST_CASE("a checkbox toggles on a click and on CONFIRM, and reports it") {
  GuiCheckbox box;
  bool last = false;
  box.on_change = [&](bool on) {
    last = on;
  };
  box.handleClick({});
  CHECK(box.state == GuiCheckState::CHECKED);
  CHECK(last);
  box.handleNav(GuiNavCommand::CONFIRM);
  CHECK(box.state == GuiCheckState::UNCHECKED);
  box.state = GuiCheckState::INDETERMINATE;
  box.toggle();
  CHECK(box.state == GuiCheckState::CHECKED);
  box.disabled = true;
  CHECK_FALSE(box.handleClick({}));
  CHECK(box.tree_focusable);
}

TEST_CASE("a checkbox measures to its box and label") {
  GuiCheckbox box;
  box.label = "Grid";
  const GuiDrawContext no_font{};
  CHECK(box.measureContent(no_font, -1).w ==
        16.0f + 8.0f + no_font.measureText("Grid"));
}

TEST_CASE("a toggle flips, and LEFT and RIGHT set it") {
  GuiToggle toggle;
  int changes = 0;
  toggle.on_change = [&](bool) {
    ++changes;
  };
  toggle.handleClick({});
  CHECK(toggle.on);
  CHECK(toggle.handleNav(GuiNavCommand::RIGHT));  // already on
  CHECK(toggle.on);
  toggle.handleNav(GuiNavCommand::LEFT);
  CHECK_FALSE(toggle.on);
  CHECK(changes == 2);
}

TEST_CASE("a radio group chooses by click and by UP and DOWN") {
  GuiRadioGroup group;
  group.options = {"Easy", "Normal", "Hard"};
  group.rect = {0, 0, 200, 84};
  int chosen = -1;
  group.on_change = [&](int i) {
    chosen = i;
  };
  group.handleClick({.x = 10, .y = 40});
  CHECK(group.selected == 1);
  CHECK(chosen == 1);
  group.handleNav(GuiNavCommand::DOWN);
  CHECK(group.selected == 2);
  CHECK_FALSE(group.handleNav(GuiNavCommand::DOWN));
  CHECK(group.optionAt(10, 90) == -1);
}

TEST_CASE("tabs select by click, where they were last drawn, and by keys") {
  GuiTabs tabs;
  tabs.tabs = {"General", "Audio", "Video"};
  tabs.rect = {0, 0, 400, 40};
  const GuiDrawContext no_font{};
  tabs.update(no_font, 0.016f);
  int picked = -1;
  tabs.on_change = [&](int i) {
    picked = i;
  };
  const float audio_x = no_font.measureText("General") + 28.0f + 5.0f;
  tabs.handleClick({.x = audio_x, .y = 10});
  CHECK(tabs.selected == 1);
  CHECK(picked == 1);
  tabs.handleNav(GuiNavCommand::RIGHT);
  CHECK(tabs.selected == 2);
  CHECK(tabs.tabAt(no_font, 5, 10) == 0);
}
