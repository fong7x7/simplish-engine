#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>
#include <cmath>
#include <engine/input/gamepad-actions.h>
#include <engine/input/player-input-builder.h>

using Catch::Approx;
using eng::Vec2;
using namespace eng::input;

TEST_CASE("a stick inside its deadzone is at rest") {
  const Vec2 out = applyStickDeadzone({0.1F, 0.1F}, 0.2F);
  REQUIRE(out.x == 0.0F);
  REQUIRE(out.y == 0.0F);
}

TEST_CASE("the deadzone is radial, so a diagonal is not dead") {
  // 0.15 on each axis is under a per-axis 0.2, but 0.21 along the stick.
  const Vec2 out = applyStickDeadzone({0.15F, 0.15F}, 0.2F);
  REQUIRE(out.x > 0.0F);
  REQUIRE(out.x == Approx(out.y));
}

TEST_CASE("travel past the deadzone is rescaled to start from zero") {
  REQUIRE(applyStickDeadzone({0.6F, 0.0F}, 0.2F).x == Approx(0.5F));
  REQUIRE(applyStickDeadzone({1.0F, 0.0F}, 0.2F).x == Approx(1.0F));
  // A square gate's corner is still no more than full length.
  const Vec2 corner = applyStickDeadzone({1.0F, 1.0F}, 0.2F);
  REQUIRE(std::hypot(corner.x, corner.y) == Approx(1.0F));
}

TEST_CASE("a trigger's deadzone comes off the bottom") {
  REQUIRE(applyTriggerDeadzone(0.05F, 0.1F) == 0.0F);
  REQUIRE(applyTriggerDeadzone(0.55F, 0.1F) == Approx(0.5F));
  REQUIRE(applyTriggerDeadzone(1.0F, 0.1F) == Approx(1.0F));
}

TEST_CASE("a pad asks for actions through its bindings") {
  GamepadState pad;
  pad.setAxis(GamepadAxis::LEFT_X, -1.0F);
  pad.setAxis(GamepadAxis::RIGHT_TRIGGER, 1.0F);
  pad.press(GamepadButton::DPAD_UP);
  ActionValues values;
  offerGamepad(values, pad, defaultGamepadBindings());
  REQUIRE(values.value(InputAction::MOVE_LEFT) == Approx(1.0F));
  REQUIRE(values.value(InputAction::MOVE_RIGHT) == 0.0F);
  REQUIRE(values.value(InputAction::MOVE_UP) == 1.0F);
  REQUIRE(values.pressed(InputAction::FIRE));
}

TEST_CASE("a resting pad asks for nothing") {
  GamepadState pad;
  pad.setAxis(GamepadAxis::LEFT_X, 0.1F);
  pad.setAxis(GamepadAxis::RIGHT_TRIGGER, 0.05F);
  ActionValues values;
  offerGamepad(values, pad, defaultGamepadBindings());
  for (std::size_t i = 0; i < INPUT_ACTION_COUNT; ++i) {
    REQUIRE(values.value(static_cast<InputAction>(i)) == 0.0F);
  }
}

TEST_CASE("rebinding fire to a face button moves it off the trigger") {
  InputBindings bindings = defaultGamepadBindings();
  bindings.clear(InputAction::FIRE);
  bindings.bind(InputAction::FIRE, InputSource::button(GamepadButton::SOUTH));
  GamepadState pad;
  pad.setAxis(GamepadAxis::RIGHT_TRIGGER, 1.0F);
  ActionValues values;
  offerGamepad(values, pad, bindings);
  REQUIRE_FALSE(values.pressed(InputAction::FIRE));
  pad.press(GamepadButton::SOUTH);
  offerGamepad(values, pad, bindings);
  REQUIRE(values.pressed(InputAction::FIRE));
}

TEST_CASE("a half-pushed stick walks at half speed") {
  GamepadState pad;
  pad.setAxis(GamepadAxis::LEFT_Y, -0.6F);
  ActionValues values;
  offerGamepad(values, pad, defaultGamepadBindings());
  const auto input = makePlayerInput(values, {}, MoveBasis{});
  REQUIRE(input.move_x == 0);
  REQUIRE(input.move_y == quantizeInputAxis(-0.5F));
}
