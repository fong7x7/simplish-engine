#include <catch2/catch_test_macros.hpp>
#include <engine/input/action-values.h>

using eng::input::ActionValues;
using eng::input::HeldActions;
using eng::input::InputAction;

TEST_CASE("held actions are at full strength and the rest at zero") {
  HeldActions held;
  held.press(InputAction::MOVE_UP);
  const ActionValues values{held};
  REQUIRE(values.value(InputAction::MOVE_UP) == 1.0F);
  REQUIRE(values.value(InputAction::MOVE_DOWN) == 0.0F);
}

TEST_CASE("the strongest offer wins rather than their sum") {
  ActionValues values;
  values.offer(InputAction::MOVE_RIGHT, 0.4F);
  values.offer(InputAction::MOVE_RIGHT, 0.7F);
  values.offer(InputAction::MOVE_RIGHT, 0.2F);
  REQUIRE(values.value(InputAction::MOVE_RIGHT) == 0.7F);
  values.offer(InputAction::MOVE_RIGHT, 5.0F);
  REQUIRE(values.value(InputAction::MOVE_RIGHT) == 1.0F);
}

TEST_CASE("an on-or-off action is pressed from halfway") {
  ActionValues values;
  values.offer(InputAction::FIRE, 0.49F);
  REQUIRE_FALSE(values.pressed(InputAction::FIRE));
  values.offer(InputAction::FIRE, 0.5F);
  REQUIRE(values.pressed(InputAction::FIRE));
}
