#include <catch2/catch_test_macros.hpp>
#include <engine/input/gamepads.h>

using eng::input::GamepadButton;
using eng::input::Gamepads;
using eng::input::WindowFocus;

// Whatever backend is compiled in, and whatever pads are plugged into the
// machine running the tests: these hold with none, and with any.

TEST_CASE("pads poll to nothing before the backend is open") {
  Gamepads pads;
  pads.poll();
  REQUIRE(pads.pads().size() == 0);
  REQUIRE(pads.pads().active() == nullptr);
}

TEST_CASE("the backend opens, polls, and closes twice without harm") {
  Gamepads pads;
  REQUIRE_FALSE(pads.open().has_value());
  REQUIRE_FALSE(pads.open().has_value());
  pads.poll();
  REQUIRE((pads.pads().size() == 0) == (pads.pads().active() == nullptr));
  pads.close();
  pads.close();
  REQUIRE(pads.pads().size() == 0);
}

TEST_CASE("an unfocused window reads every pad as resting") {
  Gamepads pads;
  REQUIRE_FALSE(pads.open().has_value());
  pads.setFocus(WindowFocus::UNFOCUSED);
  pads.poll();
  if (const auto* pad = pads.pads().active()) {
    REQUIRE_FALSE(pad->held(GamepadButton::SOUTH));
    REQUIRE_FALSE(pads.pads().pressed(GamepadButton::START));
  }
}
