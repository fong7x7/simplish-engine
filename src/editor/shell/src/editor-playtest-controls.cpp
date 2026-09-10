#include <cmath>
#include <editor/shell/editor-playtest-controls.h>
#include <editor/shell/iso-projection.h>
#include <engine/client/desktop-platform-keycode.h>

namespace eng::editor {

namespace {

  using Keycode = eng::client::DesktopPlatformKeycode;

  /// One key a playtest reads, and the action it holds.
  struct PlaytestKey {
    uint32_t key;
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

  /// The unit ground direction that @p screen, a direction on the screen,
  /// lies along under @p axes.
  Vec2 groundDirection(const IsoAxes& axes, IsoPoint screen) {
    const WorldPoint world = isoToWorld(axes, screen);
    const float length = std::sqrt(world.x * world.x + world.y * world.y);
    return length > 0.0f ? Vec2{world.x / length, world.y / length} : Vec2{};
  }

}  // namespace

std::optional<input::InputAction> editorPlaytestAction(uint32_t key) {
  for (const PlaytestKey& binding : PLAYTEST_KEYS) {
    if (binding.key == key) {
      return binding.action;
    }
  }
  return std::nullopt;
}

input::MoveBasis editorMoveBasis(const IsoAxes& axes) {
  return {groundDirection(axes, {1.0f, 0.0f}),
          groundDirection(axes, {0.0f, 1.0f})};
}

}  // namespace eng::editor
