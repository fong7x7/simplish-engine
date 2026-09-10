#include <catch2/catch_test_macros.hpp>
#include <cmath>
#include <engine/input/held-actions.h>
#include <engine/input/player-input-builder.h>
#include <limits>

using eng::Vec2;
using eng::input::HeldActions;
using eng::input::INPUT_AXIS_MAX;
using eng::input::INPUT_BUTTON_FIRE;
using eng::input::InputAction;
using eng::input::makePlayerInput;
using eng::input::MoveBasis;
using eng::input::quantizeInputAxis;

namespace {

HeldActions holding(std::initializer_list<InputAction> actions) {
  HeldActions held;
  for (const InputAction action : actions) {
    held.press(action);
  }
  return held;
}

}  // namespace

TEST_CASE("an axis is quantised to the nearest step and clamped") {
  REQUIRE(quantizeInputAxis(1.0F) == INPUT_AXIS_MAX);
  REQUIRE(quantizeInputAxis(-1.0F) == -INPUT_AXIS_MAX);
  REQUIRE(quantizeInputAxis(0.0F) == 0);
  REQUIRE(quantizeInputAxis(3.0F) == INPUT_AXIS_MAX);
  REQUIRE(quantizeInputAxis(0.5F) == 16384);
  REQUIRE(quantizeInputAxis(std::numeric_limits<float>::quiet_NaN()) == 0);
}

TEST_CASE("nothing held moves nowhere and fires nothing") {
  const auto input = makePlayerInput(HeldActions{}, Vec2{}, MoveBasis{});
  REQUIRE(input == eng::sim::PlayerInput{});
}

TEST_CASE("under the default basis, up the screen is world -Y") {
  const auto up =
      makePlayerInput(holding({InputAction::MOVE_UP}), Vec2{}, MoveBasis{});
  const auto right =
      makePlayerInput(holding({InputAction::MOVE_RIGHT}), Vec2{}, MoveBasis{});
  REQUIRE(up.move_x == 0);
  REQUIRE(up.move_y == -INPUT_AXIS_MAX);
  REQUIRE(right.move_x == INPUT_AXIS_MAX);
  REQUIRE(right.move_y == 0);
}

TEST_CASE("a diagonal is no faster than a straight line") {
  const auto input = makePlayerInput(
      holding({InputAction::MOVE_DOWN, InputAction::MOVE_RIGHT}), Vec2{},
      MoveBasis{});
  const double length = std::hypot(input.move_x, input.move_y);
  REQUIRE(std::abs(length - INPUT_AXIS_MAX) < 2.0);
  REQUIRE(input.move_x == input.move_y);
}

TEST_CASE("opposite directions held together cancel") {
  const auto input = makePlayerInput(
      holding({InputAction::MOVE_LEFT, InputAction::MOVE_RIGHT}), Vec2{},
      MoveBasis{});
  REQUIRE(input.move_x == 0);
}

TEST_CASE("the aim is a direction whatever its length") {
  const auto far =
      makePlayerInput(HeldActions{}, Vec2{30.0F, 0.0F}, MoveBasis{});
  const auto near =
      makePlayerInput(HeldActions{}, Vec2{0.25F, 0.0F}, MoveBasis{});
  REQUIRE(far.aim_x == INPUT_AXIS_MAX);
  REQUIRE(far.aim_x == near.aim_x);
  REQUIRE(far.aim_y == 0);
}

TEST_CASE("holding fire sets the fire button") {
  const auto input =
      makePlayerInput(holding({InputAction::FIRE}), Vec2{}, MoveBasis{});
  REQUIRE(input.buttons == INPUT_BUTTON_FIRE);
}

namespace {

/// The 45 degree yaw: screen-right runs along +X-Y, screen-down along +X+Y.
constexpr float HALF_ROOT2 = 0.70710678F;
constexpr MoveBasis ISOMETRIC{{HALF_ROOT2, -HALF_ROOT2},
                              {HALF_ROOT2, HALF_ROOT2}};

}  // namespace

TEST_CASE("movement follows the basis, so up is up the screen") {
  const auto up =
      makePlayerInput(holding({InputAction::MOVE_UP}), Vec2{}, ISOMETRIC);
  const auto right =
      makePlayerInput(holding({InputAction::MOVE_RIGHT}), Vec2{}, ISOMETRIC);

  // Up the screen is a diagonal across the grid: -X and -Y together.
  REQUIRE(up.move_x == -23170);
  REQUIRE(up.move_y == -23170);
  REQUIRE(right.move_x == 23170);
  REQUIRE(right.move_y == -23170);
}

TEST_CASE("a screen diagonal through a rotated basis is still full speed") {
  // Up and right on the screen is straight along world -Y under 45° of yaw.
  const auto input =
      makePlayerInput(holding({InputAction::MOVE_UP, InputAction::MOVE_RIGHT}),
                      Vec2{}, ISOMETRIC);

  REQUIRE(std::abs(input.move_x) <= 1);
  REQUIRE(input.move_y == -INPUT_AXIS_MAX);
}

TEST_CASE("the basis turns movement, not the aim") {
  const auto input =
      makePlayerInput(HeldActions{}, Vec2{1.0F, 0.0F}, ISOMETRIC);
  REQUIRE(input.aim_x == INPUT_AXIS_MAX);
  REQUIRE(input.aim_y == 0);
}
