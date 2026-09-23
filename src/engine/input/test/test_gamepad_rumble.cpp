#include <catch2/catch_test_macros.hpp>
#include <engine/input/gamepad-rumble.h>

using eng::input::GamepadRumble;
using eng::input::isRumbling;
using eng::input::strongerRumble;

TEST_CASE("a rumble with no time or no strength does nothing") {
  REQUIRE_FALSE(isRumbling(GamepadRumble{}));
  REQUIRE_FALSE(isRumbling({.low = 1.0F}));
  REQUIRE_FALSE(isRumbling({.seconds = 1.0F}));
  REQUIRE(isRumbling({.right_trigger = 0.2F, .seconds = 0.1F}));
}

TEST_CASE("two rumbles together are the stronger of each, not the sum") {
  const GamepadRumble shot{
      .high = 0.3F, .right_trigger = 0.4F, .seconds = 0.05F};
  const GamepadRumble blast{.low = 0.8F, .high = 0.2F, .seconds = 0.3F};
  const GamepadRumble both = strongerRumble(shot, blast);
  REQUIRE(both.low == 0.8F);
  REQUIRE(both.high == 0.3F);
  REQUIRE(both.right_trigger == 0.4F);
  REQUIRE(both.seconds == 0.3F);
}
