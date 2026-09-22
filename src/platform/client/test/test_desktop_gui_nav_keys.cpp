#include <catch2/catch_test_macros.hpp>
#include <engine/client/desktop-gui-nav-keys.h>
#include <engine/client/desktop-platform-keycode.h>

using eng::GuiNavCommand;
using eng::client::desktopGuiNavCommand;
using Keycode = eng::client::DesktopPlatformKeycode;

TEST_CASE("arrows move, Enter and Space confirm, Escape cancels") {
  REQUIRE(desktopGuiNavCommand({.key = Keycode::ARROW_LEFT}) ==
          GuiNavCommand::LEFT);
  REQUIRE(desktopGuiNavCommand({.key = Keycode::ARROW_DOWN}) ==
          GuiNavCommand::DOWN);
  REQUIRE(desktopGuiNavCommand({.key = Keycode::KEY_RETURN}) ==
          GuiNavCommand::CONFIRM);
  REQUIRE(desktopGuiNavCommand({.key = Keycode::SPACE}) ==
          GuiNavCommand::CONFIRM);
  REQUIRE(desktopGuiNavCommand({.key = Keycode::ESCAPE}) ==
          GuiNavCommand::CANCEL);
  REQUIRE_FALSE(desktopGuiNavCommand({.key = 'w'}).has_value());
}

TEST_CASE("Tab steps forward and Shift+Tab back") {
  REQUIRE(desktopGuiNavCommand({.key = Keycode::TAB}) == GuiNavCommand::NEXT);
  REQUIRE(desktopGuiNavCommand({.key = Keycode::TAB, .shift = true}) ==
          GuiNavCommand::PREVIOUS);
}

TEST_CASE("while typing, only Escape and Tab navigate") {
  REQUIRE_FALSE(
      desktopGuiNavCommand({.key = Keycode::ARROW_LEFT, .typing = true})
          .has_value());
  REQUIRE_FALSE(desktopGuiNavCommand({.key = Keycode::SPACE, .typing = true})
                    .has_value());
  REQUIRE(desktopGuiNavCommand({.key = Keycode::ESCAPE, .typing = true}) ==
          GuiNavCommand::CANCEL);
  REQUIRE(desktopGuiNavCommand({.key = Keycode::TAB, .typing = true}) ==
          GuiNavCommand::NEXT);
}

TEST_CASE("held arrows repeat, but a held Enter confirms only once") {
  REQUIRE(desktopGuiNavCommand({.key = Keycode::ARROW_UP, .repeat = true}) ==
          GuiNavCommand::UP);
  REQUIRE_FALSE(
      desktopGuiNavCommand({.key = Keycode::KEY_RETURN, .repeat = true})
          .has_value());
  REQUIRE_FALSE(desktopGuiNavCommand({.key = Keycode::ESCAPE, .repeat = true})
                    .has_value());
}
