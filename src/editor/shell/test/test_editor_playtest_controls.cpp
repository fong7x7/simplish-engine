#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>
#include <editor/shell/editor-playtest-controls.h>
#include <editor/shell/iso-projection.h>
#include <engine/client/desktop-platform-keycode.h>
#include <engine/input/gamepad-actions.h>
#include <engine/input/held-actions.h>
#include <engine/input/player-input-builder.h>

using Catch::Approx;
using namespace eng;
using namespace eng::editor;

namespace {

/// Where the ground direction @p direction lands on screen under @p axes.
IsoPoint onScreen(const IsoAxes& axes, Vec2 direction) {
  return worldToIso(axes, {direction.x, direction.y, 0.0f});
}

/// The actions the default scheme binds @p key to.
std::vector<input::InputAction> actionsForKey(uint32_t key) {
  return editorDefaultInputBindings().actionsFor(input::InputSource::key(key));
}

/// Player 1's input while holding the one key @p key under @p axes.
sim::PlayerInput holdingKey(uint32_t key, const IsoAxes& axes) {
  input::HeldActions held;
  for (const input::InputAction action : actionsForKey(key)) {
    held.press(action);
  }
  return input::makePlayerInput(held, {}, editorMoveBasis(axes));
}

}  // namespace

TEST_CASE("WASD and the arrows move; other keys do nothing") {
  using Keycode = eng::client::DesktopPlatformKeycode;
  using A = input::InputAction;
  REQUIRE(actionsForKey('w') == std::vector{A::MOVE_UP});
  REQUIRE(actionsForKey(Keycode::ARROW_UP) == std::vector{A::MOVE_UP});
  REQUIRE(actionsForKey('d') == std::vector{A::MOVE_RIGHT});
  REQUIRE(actionsForKey(Keycode::ARROW_LEFT) == std::vector{A::MOVE_LEFT});
  REQUIRE(actionsForKey('g').empty());
}

TEST_CASE("every pad plays twin-stick by default") {
  const input::InputBindings bindings = editorDefaultInputBindings();
  input::GamepadState pad;
  pad.setAxis(input::GamepadAxis::LEFT_X, 1.0f);
  pad.setAxis(input::GamepadAxis::RIGHT_Y, -1.0f);
  pad.setAxis(input::GamepadAxis::RIGHT_TRIGGER, 1.0f);
  input::ActionValues values;
  input::offerGamepad(values, pad, bindings);
  const sim::PlayerInput played =
      input::makePlayerInput(values, {}, editorMoveBasis(ISO_AXES_DIMETRIC));
  REQUIRE(played.move_x == input::INPUT_AXIS_MAX);
  REQUIRE(played.aim_y == -input::INPUT_AXIS_MAX);
  REQUIRE(played.buttons == input::INPUT_BUTTON_FIRE);
}

TEST_CASE("the pad drives the character selector as the keys do") {
  using Keycode = eng::client::DesktopPlatformKeycode;
  using B = input::GamepadButton;
  REQUIRE(editorChoosingKeyFor(B::DPAD_RIGHT) == Keycode::ARROW_RIGHT);
  REQUIRE(editorChoosingKeyFor(B::DPAD_UP) == Keycode::ARROW_UP);
  REQUIRE(editorChoosingKeyFor(B::SOUTH) == Keycode::KEY_RETURN);
  REQUIRE(editorChoosingKeyFor(B::EAST) == Keycode::ESCAPE);
  REQUIRE_FALSE(editorChoosingKeyFor(B::START).has_value());
}

TEST_CASE("under the dimetric view the screen and world axes agree") {
  const input::MoveBasis basis = editorMoveBasis(ISO_AXES_DIMETRIC);

  REQUIRE(basis.right.x == Approx(1.0f));
  REQUIRE(basis.right.y == Approx(0.0f).margin(1e-6));
  REQUIRE(basis.down.x == Approx(0.0f).margin(1e-6));
  REQUIRE(basis.down.y == Approx(1.0f));
}

TEST_CASE("under the isometric view up the screen is a grid diagonal") {
  const input::MoveBasis basis = editorMoveBasis(ISO_AXES_ISOMETRIC);

  // Screen-down is +X and +Y together, so W — up — is -X and -Y.
  REQUIRE(basis.down.x == Approx(0.70710678f));
  REQUIRE(basis.down.y == Approx(0.70710678f));
  REQUIRE(basis.right.x == Approx(0.70710678f));
  REQUIRE(basis.right.y == Approx(-0.70710678f));

  const sim::PlayerInput up = holdingKey('w', ISO_AXES_ISOMETRIC);
  REQUIRE(up.move_x < 0);
  REQUIRE(up.move_x == up.move_y);
}

TEST_CASE("each key moves straight along its screen direction in either view") {
  for (const IsoAxes& axes : {ISO_AXES_DIMETRIC, ISO_AXES_ISOMETRIC}) {
    const input::MoveBasis basis = editorMoveBasis(axes);
    const IsoPoint right = onScreen(axes, basis.right);
    const IsoPoint down = onScreen(axes, basis.down);

    // Right lands straight across the screen, down straight down it.
    REQUIRE(right.x > 0.0f);
    REQUIRE(right.y == Approx(0.0f).margin(1e-4));
    REQUIRE(down.x == Approx(0.0f).margin(1e-4));
    REQUIRE(down.y > 0.0f);
    // Unit and perpendicular on the ground, so every push is full speed.
    REQUIRE(basis.right.x * basis.right.x + basis.right.y * basis.right.y ==
            Approx(1.0f));
    REQUIRE(basis.right.x * basis.down.x + basis.right.y * basis.down.y ==
            Approx(0.0f).margin(1e-6));
  }
}
