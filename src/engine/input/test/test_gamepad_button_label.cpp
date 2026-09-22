#include <array>
#include <catch2/catch_test_macros.hpp>
#include <engine/input/gamepad-button-label.h>
#include <engine/input/gamepad-set.h>

using namespace eng::input;

TEST_CASE("the bottom face button is labelled by whose pad it is") {
  REQUIRE(gamepadButtonLabel(GamepadButton::SOUTH, GamepadFamily::XBOX) == "A");
  REQUIRE(gamepadButtonLabel(GamepadButton::SOUTH,
                             GamepadFamily::PLAYSTATION) == "Cross");
  REQUIRE(gamepadButtonLabel(GamepadButton::SOUTH, GamepadFamily::NINTENDO) ==
          "B");
  REQUIRE(gamepadButtonLabel(GamepadButton::EAST, GamepadFamily::NINTENDO) ==
          "A");
}

TEST_CASE("a generic pad is labelled as an Xbox pad") {
  REQUIRE(gamepadButtonLabel(GamepadButton::NORTH, GamepadFamily::GENERIC) ==
          "Y");
  REQUIRE(gamepadAxisLabel(GamepadAxis::RIGHT_TRIGGER,
                           GamepadFamily::GENERIC) == "RT");
}

TEST_CASE("triggers and shoulders carry each family's names") {
  REQUIRE(gamepadAxisLabel(GamepadAxis::RIGHT_TRIGGER,
                           GamepadFamily::PLAYSTATION) == "R2");
  REQUIRE(gamepadAxisLabel(GamepadAxis::LEFT_TRIGGER,
                           GamepadFamily::NINTENDO) == "ZL");
  REQUIRE(gamepadButtonLabel(GamepadButton::RIGHT_SHOULDER,
                             GamepadFamily::PLAYSTATION) == "R1");
}

TEST_CASE("every button has a label in every family") {
  for (const GamepadFamily family :
       {GamepadFamily::GENERIC, GamepadFamily::XBOX, GamepadFamily::PLAYSTATION,
        GamepadFamily::NINTENDO}) {
    for (std::size_t i = 0; i < GAMEPAD_BUTTON_COUNT; ++i) {
      REQUIRE_FALSE(
          gamepadButtonLabel(static_cast<GamepadButton>(i), family).empty());
    }
  }
}

TEST_CASE("the pad in use reports its family") {
  GamepadSet pads;
  REQUIRE(pads.activeFamily() == GamepadFamily::GENERIC);
  GamepadReading ps{1, {}, GamepadFamily::PLAYSTATION};
  pads.update(std::array{ps});
  REQUIRE(pads.activeFamily() == GamepadFamily::PLAYSTATION);
}
