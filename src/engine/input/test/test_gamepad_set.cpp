#include <array>
#include <catch2/catch_test_macros.hpp>
#include <engine/input/gamepad-set.h>

using namespace eng::input;

namespace {

/// A pad reading with @p button held.
GamepadReading holding(uint64_t device, GamepadButton button) {
  GamepadReading reading{device, {}};
  reading.state.press(button);
  return reading;
}

/// A pad reading with its left stick at @p x.
GamepadReading pushing(uint64_t device, float x) {
  GamepadReading reading{device, {}};
  reading.state.setAxis(GamepadAxis::LEFT_X, x);
  return reading;
}

}  // namespace

TEST_CASE("with no pads there is none in use") {
  GamepadSet pads;
  pads.update({});
  REQUIRE(pads.size() == 0);
  REQUIRE(pads.active() == nullptr);
  REQUIRE_FALSE(pads.pressed(GamepadButton::SOUTH));
}

TEST_CASE("the first pad is in use until another is touched") {
  GamepadSet pads;
  pads.update(std::array{pushing(1, 0.0F), pushing(2, 0.0F)});
  REQUIRE(pads.active()->axis(GamepadAxis::LEFT_X) == 0.0F);

  pads.update(std::array{pushing(1, 0.0F), pushing(2, 0.9F)});
  REQUIRE(pads.active()->axis(GamepadAxis::LEFT_X) == 0.9F);

  // Pad 1 drifting under the threshold does not take it back.
  pads.update(std::array{pushing(1, 0.3F), pushing(2, 0.0F)});
  REQUIRE(pads.active()->axis(GamepadAxis::LEFT_X) == 0.0F);
}

TEST_CASE("a press is reported once, on the frame it goes down") {
  GamepadSet pads;
  pads.update(std::array{holding(7, GamepadButton::SOUTH)});
  REQUIRE(pads.pressed(GamepadButton::SOUTH));
  pads.update(std::array{holding(7, GamepadButton::SOUTH)});
  REQUIRE_FALSE(pads.pressed(GamepadButton::SOUTH));
  REQUIRE(pads.active()->held(GamepadButton::SOUTH));
}

TEST_CASE("when the pad in use is unplugged, the next one takes over") {
  GamepadSet pads;
  pads.update(std::array{pushing(1, 0.0F), holding(2, GamepadButton::EAST)});
  REQUIRE(pads.active()->held(GamepadButton::EAST));
  pads.update(std::array{pushing(1, 0.25F)});
  REQUIRE(pads.size() == 1);
  REQUIRE(pads.active()->axis(GamepadAxis::LEFT_X) == 0.25F);
}

TEST_CASE("a pad plugged back in starts from all released") {
  GamepadSet pads;
  pads.update(std::array{holding(3, GamepadButton::START)});
  pads.update({});
  pads.update(std::array{holding(3, GamepadButton::START)});
  REQUIRE(pads.pressed(GamepadButton::START));
}

TEST_CASE("a pad is touched on the frame it is picked up, not after") {
  GamepadSet pads;
  pads.update(std::array{pushing(1, 0.0F)});
  REQUIRE_FALSE(pads.touched());
  pads.update(std::array{pushing(1, 0.9F)});
  REQUIRE(pads.touched());
  pads.update(std::array{pushing(1, 0.9F)});
  REQUIRE_FALSE(pads.touched());
}
