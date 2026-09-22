#include <catch2/catch_test_macros.hpp>
#include <editor/shell/editor-controls-ops.h>
#include <editor/shell/editor-playtest-controls.h>
#include <engine/client/desktop-platform-keycode.h>

using namespace eng;
using namespace eng::editor;
using input::GamepadAxis;
using input::GamepadButton;
using input::GamepadFamily;
using input::InputAction;
using input::InputSource;

TEST_CASE(
    "rebinding a key keeps the action's pad controls, and the other way") {
  input::InputBindings bindings = editorDefaultInputBindings();
  editorRebind(bindings, InputAction::MOVE_UP, InputSource::key('i'));
  REQUIRE(editorActionKeysLabel(bindings, InputAction::MOVE_UP) == "I");
  REQUIRE(editorActionControlsLabel(bindings, InputAction::MOVE_UP,
                                    GamepadFamily::XBOX) ==
          "Left Stick Up / D-pad Up");

  editorRebind(bindings, InputAction::MOVE_UP,
               InputSource::button(GamepadButton::NORTH));
  REQUIRE(editorActionControlsLabel(bindings, InputAction::MOVE_UP,
                                    GamepadFamily::PLAYSTATION) == "Triangle");
  REQUIRE(editorActionKeysLabel(bindings, InputAction::MOVE_UP) == "I");
}

TEST_CASE("controls are labelled as the key or pad prints them") {
  using Keycode = client::DesktopPlatformKeycode;
  REQUIRE(editorControlLabel(InputSource::key(Keycode::ARROW_UP),
                             GamepadFamily::GENERIC) == "Up");
  REQUIRE(editorControlLabel(InputSource::key(' '), GamepadFamily::GENERIC) ==
          "Space");
  REQUIRE(editorControlLabel(InputSource::positive(GamepadAxis::RIGHT_TRIGGER),
                             GamepadFamily::NINTENDO) == "ZR");
  REQUIRE(editorControlLabel(InputSource::negative(GamepadAxis::RIGHT_X),
                             GamepadFamily::XBOX) == "Right Stick Left");
}

TEST_CASE("an action with nothing on a device shows a dash") {
  input::InputBindings bindings;
  REQUIRE(editorActionKeysLabel(bindings, InputAction::FIRE) == "—");
  REQUIRE(editorActionLabel(InputAction::AIM_LEFT) == "Aim left");
}
