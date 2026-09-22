#include <cmath>
#include <editor/shell/editor-playtest-controls.h>
#include <editor/shell/iso-projection.h>
#include <engine/client/desktop-platform-keycode.h>

namespace eng::editor {

namespace {

  using Keycode = eng::client::DesktopPlatformKeycode;
  using Button = input::GamepadButton;

  /// One key a playtest reads, and the action it holds.
  struct PlaytestKey {
    /// The key's symbol.
    uint32_t key;
    /// What holding it asks for.
    input::InputAction action;
  };

  /// WASD and the arrow keys, each named for the screen direction it moves.
  constexpr PlaytestKey PLAYTEST_KEYS[] = {
      {'w', input::InputAction::MOVE_UP},
      {Keycode::ARROW_UP, input::InputAction::MOVE_UP},
      {'s', input::InputAction::MOVE_DOWN},
      {Keycode::ARROW_DOWN, input::InputAction::MOVE_DOWN},
      {'a', input::InputAction::MOVE_LEFT},
      {Keycode::ARROW_LEFT, input::InputAction::MOVE_LEFT},
      {'d', input::InputAction::MOVE_RIGHT},
      {Keycode::ARROW_RIGHT, input::InputAction::MOVE_RIGHT},
  };

  /// One pad button the character selector reads, and the key it acts as.
  struct ChoosingButton {
    /// The button.
    Button button;
    /// The selector key it stands for.
    uint32_t key;
  };

  /// The d-pad steps, South confirms and East cancels, as the GUI's own
  /// dialogs map a pad (gui.md §4.7).
  constexpr ChoosingButton CHOOSING_BUTTONS[] = {
      {Button::DPAD_LEFT, Keycode::ARROW_LEFT},
      {Button::DPAD_UP, Keycode::ARROW_UP},
      {Button::DPAD_RIGHT, Keycode::ARROW_RIGHT},
      {Button::DPAD_DOWN, Keycode::ARROW_DOWN},
      {Button::SOUTH, Keycode::KEY_RETURN},
      {Button::EAST, Keycode::ESCAPE},
  };

  /// The unit ground direction that @p screen, a direction on the screen,
  /// lies along under @p axes.
  Vec2 groundDirection(const IsoAxes& axes, IsoPoint screen) {
    const WorldPoint world = isoToWorld(axes, screen);
    const float length = std::sqrt(world.x * world.x + world.y * world.y);
    return length > 0.0f ? Vec2{world.x / length, world.y / length} : Vec2{};
  }

}  // namespace

input::InputBindings editorDefaultInputBindings() {
  input::InputBindings bindings = input::defaultGamepadBindings();
  for (const PlaytestKey& binding : PLAYTEST_KEYS) {
    bindings.bind(binding.action, input::InputSource::key(binding.key));
  }
  return bindings;
}

std::optional<uint32_t> editorChoosingKeyFor(input::GamepadButton button) {
  for (const ChoosingButton& entry : CHOOSING_BUTTONS) {
    if (entry.button == button) {
      return entry.key;
    }
  }
  return std::nullopt;
}

input::MoveBasis editorMoveBasis(const IsoAxes& axes) {
  return {groundDirection(axes, {1.0f, 0.0f}),
          groundDirection(axes, {0.0f, 1.0f})};
}

}  // namespace eng::editor
