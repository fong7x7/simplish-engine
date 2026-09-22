#include <catch2/catch_test_macros.hpp>
#include <engine/gui/gui-gamepad-navigator.h>

using namespace eng;
using input::GamepadAxis;
using input::GamepadButton;
using input::GamepadState;

namespace {

/// A pad with @p button held.
GamepadState holding(GamepadButton button) {
  GamepadState pad;
  pad.press(button);
  return pad;
}

using Commands = std::vector<GuiNavCommand>;

constexpr input::GamepadFamily XBOX = input::GamepadFamily::XBOX;

}  // namespace

TEST_CASE("a d-pad press is one step, and nothing more until held") {
  GuiGamepadNavigator nav;
  const GamepadState down = holding(GamepadButton::DPAD_DOWN);
  REQUIRE(nav.update(&down, XBOX, 0.016f) == Commands{GuiNavCommand::DOWN});
  REQUIRE(nav.update(&down, XBOX, 0.1f).empty());
  REQUIRE(nav.update(&down, XBOX, 0.1f).empty());
}

TEST_CASE("a direction held repeats after a pause") {
  GuiGamepadNavigator nav;
  const GamepadState right = holding(GamepadButton::DPAD_RIGHT);
  (void)nav.update(&right, XBOX, 0.0f);
  REQUIRE(nav.update(&right, XBOX, 0.39f).empty());
  REQUIRE(nav.update(&right, XBOX, 0.02f) == Commands{GuiNavCommand::RIGHT});
  REQUIRE(nav.update(&right, XBOX, 0.05f).empty());
  REQUIRE(nav.update(&right, XBOX, 0.08f) == Commands{GuiNavCommand::RIGHT});
}

TEST_CASE("a long frame repeats once, not in a burst") {
  GuiGamepadNavigator nav;
  const GamepadState up = holding(GamepadButton::DPAD_UP);
  (void)nav.update(&up, XBOX, 0.0f);
  REQUIRE(nav.update(&up, XBOX, 5.0f) == Commands{GuiNavCommand::UP});
  REQUIRE(nav.update(&up, XBOX, 0.0f).empty());
}

TEST_CASE("the left stick navigates along its stronger axis") {
  GuiGamepadNavigator nav;
  GamepadState pad;
  pad.setAxis(GamepadAxis::LEFT_X, -0.8f);
  pad.setAxis(GamepadAxis::LEFT_Y, 0.3f);
  REQUIRE(nav.update(&pad, XBOX, 0.0f) == Commands{GuiNavCommand::LEFT});

  GamepadState drift;
  drift.setAxis(GamepadAxis::LEFT_Y, 0.3f);
  REQUIRE(nav.update(&drift, XBOX, 0.0f).empty());
}

TEST_CASE("changing direction steps at once") {
  GuiGamepadNavigator nav;
  const GamepadState down = holding(GamepadButton::DPAD_DOWN);
  const GamepadState left = holding(GamepadButton::DPAD_LEFT);
  (void)nav.update(&down, XBOX, 0.0f);
  REQUIRE(nav.update(&left, XBOX, 0.01f) == Commands{GuiNavCommand::LEFT});
}

TEST_CASE("south confirms, east cancels, shoulders step through order") {
  GuiGamepadNavigator nav;
  const GamepadState south = holding(GamepadButton::SOUTH);
  REQUIRE(nav.update(&south, XBOX, 0.0f) == Commands{GuiNavCommand::CONFIRM});
  REQUIRE(nav.update(&south, XBOX, 0.5f).empty());

  const GamepadState east = holding(GamepadButton::EAST);
  REQUIRE(nav.update(&east, XBOX, 0.0f) == Commands{GuiNavCommand::CANCEL});
  const GamepadState rb = holding(GamepadButton::RIGHT_SHOULDER);
  REQUIRE(nav.update(&rb, XBOX, 0.0f) == Commands{GuiNavCommand::NEXT});
  const GamepadState lb = holding(GamepadButton::LEFT_SHOULDER);
  REQUIRE(nav.update(&lb, XBOX, 0.0f) == Commands{GuiNavCommand::PREVIOUS});
}

TEST_CASE("the press that opened a menu does not also confirm in it") {
  GuiGamepadNavigator nav;
  const GamepadState south = holding(GamepadButton::SOUTH);
  nav.reset(&south);
  REQUIRE(nav.update(&south, XBOX, 0.0f).empty());
}

TEST_CASE("no pad is no commands, and a pad back starts fresh") {
  GuiGamepadNavigator nav;
  const GamepadState south = holding(GamepadButton::SOUTH);
  (void)nav.update(&south, XBOX, 0.0f);
  REQUIRE(nav.update(nullptr, input::GamepadFamily::XBOX, 0.0f).empty());
  REQUIRE(nav.update(&south, XBOX, 0.0f) == Commands{GuiNavCommand::CONFIRM});
}

TEST_CASE("a Nintendo pad confirms with A, on the right, and cancels with B") {
  GuiGamepadNavigator nav;
  const GamepadState east = holding(GamepadButton::EAST);
  REQUIRE(nav.update(&east, input::GamepadFamily::NINTENDO, 0.0f) ==
          Commands{GuiNavCommand::CONFIRM});
  const GamepadState south = holding(GamepadButton::SOUTH);
  REQUIRE(nav.update(&south, input::GamepadFamily::NINTENDO, 0.0f) ==
          Commands{GuiNavCommand::CANCEL});
}
