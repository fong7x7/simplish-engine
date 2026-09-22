#include <catch2/catch_test_macros.hpp>
#include <engine/input/input-bindings.h>

using eng::input::defaultGamepadBindings;
using eng::input::GamepadAxis;
using eng::input::GamepadButton;
using eng::input::InputAction;
using eng::input::InputBindings;
using eng::input::InputSource;

TEST_CASE("a control binds once, to as many actions as it is given") {
  InputBindings bindings;
  const InputSource south = InputSource::button(GamepadButton::SOUTH);
  bindings.bind(InputAction::FIRE, south);
  bindings.bind(InputAction::FIRE, south);
  bindings.bind(InputAction::MOVE_UP, south);
  REQUIRE(bindings.sources(InputAction::FIRE).size() == 1);
  REQUIRE(bindings.actionsFor(south) ==
          std::vector{InputAction::MOVE_UP, InputAction::FIRE});
}

TEST_CASE("unbinding and clearing take controls off an action") {
  InputBindings bindings;
  bindings.bind(InputAction::FIRE, InputSource::key('f'));
  bindings.bind(InputAction::FIRE, InputSource::key('g'));
  bindings.unbind(InputAction::FIRE, InputSource::key('f'));
  REQUIRE(bindings.sources(InputAction::FIRE).size() == 1);
  REQUIRE(bindings.sources(InputAction::FIRE)[0] == InputSource::key('g'));
  bindings.clear(InputAction::FIRE);
  REQUIRE(bindings.sources(InputAction::FIRE).empty());
}

TEST_CASE("a key and a button with the same code are different controls") {
  InputBindings bindings;
  bindings.bind(InputAction::FIRE, InputSource::key(0));
  REQUIRE(
      bindings.actionsFor(InputSource::button(GamepadButton::SOUTH)).empty());
}

TEST_CASE("deadzones are held to a range that leaves travel to steer with") {
  InputBindings bindings;
  bindings.setDeadzones({-1.0F, 2.0F, 0.3F});
  REQUIRE(bindings.deadzones().left_stick == 0.0F);
  REQUIRE(bindings.deadzones().right_stick == 0.95F);
  REQUIRE(bindings.deadzones().trigger == 0.3F);
}

TEST_CASE("the default pad scheme is twin-stick") {
  const InputBindings bindings = defaultGamepadBindings();
  REQUIRE(bindings.actionsFor(InputSource::negative(GamepadAxis::LEFT_Y)) ==
          std::vector{InputAction::MOVE_UP});
  REQUIRE(bindings.actionsFor(InputSource::button(GamepadButton::DPAD_LEFT)) ==
          std::vector{InputAction::MOVE_LEFT});
  REQUIRE(bindings.actionsFor(InputSource::positive(GamepadAxis::RIGHT_X)) ==
          std::vector{InputAction::AIM_RIGHT});
  REQUIRE(bindings.actionsFor(InputSource::positive(
              GamepadAxis::RIGHT_TRIGGER)) == std::vector{InputAction::FIRE});
  for (std::size_t i = 0; i < eng::input::INPUT_ACTION_COUNT; ++i) {
    REQUIRE_FALSE(bindings.sources(static_cast<InputAction>(i)).empty());
  }
}
