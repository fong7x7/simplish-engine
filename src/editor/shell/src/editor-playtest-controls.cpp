#include <cmath>
#include <editor/shell/editor-playtest-controls.h>
#include <editor/shell/iso-projection.h>
#include <engine/client/desktop-platform-keycode.h>
#include <engine/gui/gui-nav-buttons.h>
#include <engine/input/gamepad-actions.h>
#include <engine/input/player-input-builder.h>

namespace eng::editor {

namespace {

  using Keycode = eng::client::DesktopPlatformKeycode;

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

  /// One menu command the character selector reads, and its key.
  struct ChoosingKey {
    /// The command.
    GuiNavCommand command;
    /// The selector key it stands for.
    uint32_t key;
  };

  /// The selector's keys for each menu command a pad can make.
  constexpr ChoosingKey CHOOSING_KEYS[] = {
      {GuiNavCommand::LEFT, Keycode::ARROW_LEFT},
      {GuiNavCommand::UP, Keycode::ARROW_UP},
      {GuiNavCommand::RIGHT, Keycode::ARROW_RIGHT},
      {GuiNavCommand::DOWN, Keycode::ARROW_DOWN},
      {GuiNavCommand::CONFIRM, Keycode::KEY_RETURN},
      {GuiNavCommand::CANCEL, Keycode::ESCAPE},
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

std::optional<uint32_t> editorChoosingKeyFor(input::GamepadButton button,
                                             input::GamepadFamily family) {
  const std::optional<GuiNavCommand> command = guiNavCommandFor(button, family);
  for (const ChoosingKey& entry : CHOOSING_KEYS) {
    if (command == entry.command) {
      return entry.key;
    }
  }
  return std::nullopt;
}

sim::PlayerInput editorPadInput(const input::GamepadState& pad,
                                const input::InputBindings& bindings,
                                const input::MoveBasis& basis) {
  input::ActionValues values;
  input::offerGamepad(values, pad, bindings);
  return input::makePlayerInput(values, {}, basis);
}

uint8_t editorPadPlayers(const input::GamepadSeats& seats) {
  uint8_t top = 0;
  for (uint8_t seat = 1; seat < sim::MAX_PLAYERS; ++seat) {
    if (seats.occupied(seat)) {
      top = seat;
    }
  }
  return top;
}

input::MoveBasis editorMoveBasis(const IsoAxes& axes) {
  return {groundDirection(axes, {1.0f, 0.0f}),
          groundDirection(axes, {0.0f, 1.0f})};
}

}  // namespace eng::editor
