#include <catch2/catch_test_macros.hpp>
#include <engine/input/held-actions.h>

using eng::input::HeldActions;
using eng::input::InputAction;

TEST_CASE("held actions remember presses until released") {
  HeldActions held;
  held.press(InputAction::FIRE);
  held.press(InputAction::MOVE_UP);
  REQUIRE(held.held(InputAction::FIRE));
  held.release(InputAction::FIRE);
  REQUIRE_FALSE(held.held(InputAction::FIRE));
  REQUIRE(held.held(InputAction::MOVE_UP));
  held.releaseAll();
  REQUIRE_FALSE(held.held(InputAction::MOVE_UP));
}

TEST_CASE("pressing an action twice is still one held action") {
  HeldActions held;
  held.press(InputAction::MOVE_LEFT);
  held.press(InputAction::MOVE_LEFT);
  held.release(InputAction::MOVE_LEFT);
  REQUIRE_FALSE(held.held(InputAction::MOVE_LEFT));
}
