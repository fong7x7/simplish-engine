#include <catch2/catch_test_macros.hpp>
#include <engine/gui/gui-nav-buttons.h>

using namespace eng;
using input::GamepadButton;
using input::GamepadFamily;

TEST_CASE("the bottom button confirms, except on a Nintendo pad") {
  REQUIRE(guiNavButton(GuiNavCommand::CONFIRM, GamepadFamily::XBOX) ==
          GamepadButton::SOUTH);
  REQUIRE(guiNavButton(GuiNavCommand::CONFIRM, GamepadFamily::PLAYSTATION) ==
          GamepadButton::SOUTH);
  REQUIRE(guiNavButton(GuiNavCommand::CONFIRM, GamepadFamily::NINTENDO) ==
          GamepadButton::EAST);
  REQUIRE(guiNavButton(GuiNavCommand::CANCEL, GamepadFamily::NINTENDO) ==
          GamepadButton::SOUTH);
}

TEST_CASE("a button's command is the inverse of the command's button") {
  for (const GamepadFamily family :
       {GamepadFamily::GENERIC, GamepadFamily::XBOX, GamepadFamily::PLAYSTATION,
        GamepadFamily::NINTENDO}) {
    for (const GuiNavCommand command :
         {GuiNavCommand::UP, GuiNavCommand::DOWN, GuiNavCommand::LEFT,
          GuiNavCommand::RIGHT, GuiNavCommand::CONFIRM, GuiNavCommand::CANCEL,
          GuiNavCommand::NEXT, GuiNavCommand::PREVIOUS}) {
      REQUIRE(guiNavCommandFor(guiNavButton(command, family), family) ==
              command);
    }
  }
  REQUIRE_FALSE(
      guiNavCommandFor(GamepadButton::START, GamepadFamily::XBOX).has_value());
}

TEST_CASE("prompts name the confirm button as the pad prints it") {
  REQUIRE(guiNavPrompt(GuiNavCommand::CONFIRM, GamepadFamily::XBOX) == "A");
  REQUIRE(guiNavPrompt(GuiNavCommand::CONFIRM, GamepadFamily::PLAYSTATION) ==
          "Cross");
  REQUIRE(guiNavPrompt(GuiNavCommand::CONFIRM, GamepadFamily::NINTENDO) == "A");
  REQUIRE(guiNavPrompt(GuiNavCommand::CANCEL, GamepadFamily::PLAYSTATION) ==
          "Circle");
  REQUIRE(guiNavPrompt(GuiNavCommand::NEXT, GamepadFamily::NINTENDO) == "R");
}
