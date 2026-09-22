#include <catch2/catch_test_macros.hpp>
#include <engine/input/gamepad-state.h>
#include <limits>

using eng::input::GamepadAxis;
using eng::input::GamepadButton;
using eng::input::GamepadState;

TEST_CASE("a pad's buttons are held until released") {
  GamepadState pad;
  pad.press(GamepadButton::SOUTH);
  pad.press(GamepadButton::TOUCHPAD);
  REQUIRE(pad.held(GamepadButton::SOUTH));
  REQUIRE(pad.held(GamepadButton::TOUCHPAD));
  REQUIRE_FALSE(pad.held(GamepadButton::EAST));

  pad.release(GamepadButton::SOUTH);
  REQUIRE_FALSE(pad.held(GamepadButton::SOUTH));
  REQUIRE(pad.held(GamepadButton::TOUCHPAD));
}

TEST_CASE("sticks run -1 to 1 and triggers 0 to 1") {
  GamepadState pad;
  pad.setAxis(GamepadAxis::LEFT_X, -3.0F);
  pad.setAxis(GamepadAxis::RIGHT_Y, 0.25F);
  pad.setAxis(GamepadAxis::LEFT_TRIGGER, -0.5F);
  pad.setAxis(GamepadAxis::RIGHT_TRIGGER, 2.0F);
  REQUIRE(pad.axis(GamepadAxis::LEFT_X) == -1.0F);
  REQUIRE(pad.axis(GamepadAxis::RIGHT_Y) == 0.25F);
  REQUIRE(pad.axis(GamepadAxis::LEFT_TRIGGER) == 0.0F);
  REQUIRE(pad.axis(GamepadAxis::RIGHT_TRIGGER) == 1.0F);
}

TEST_CASE("a NaN axis reads as resting") {
  GamepadState pad;
  pad.setAxis(GamepadAxis::LEFT_Y, std::numeric_limits<float>::quiet_NaN());
  REQUIRE(pad.axis(GamepadAxis::LEFT_Y) == 0.0F);
}

TEST_CASE("clearing a pad releases and centres everything") {
  GamepadState pad;
  pad.press(GamepadButton::START);
  pad.setAxis(GamepadAxis::LEFT_X, 1.0F);
  pad.clear();
  REQUIRE_FALSE(pad.held(GamepadButton::START));
  REQUIRE(pad.axis(GamepadAxis::LEFT_X) == 0.0F);
}
