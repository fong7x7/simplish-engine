#include <engine/client/desktop-gui-nav-keys.h>
#include <engine/client/desktop-platform-keycode.h>

namespace eng::client {

namespace {

  using Keycode = DesktopPlatformKeycode;

  /// One key and the command it makes.
  struct NavKey {
    /// The key symbol.
    uint32_t key;
    /// The command it asks for.
    GuiNavCommand command;
  };

  /// The keys that navigate only while nothing is typing.
  constexpr NavKey MENU_KEYS[] = {
      {Keycode::ARROW_UP, GuiNavCommand::UP},
      {Keycode::ARROW_DOWN, GuiNavCommand::DOWN},
      {Keycode::ARROW_LEFT, GuiNavCommand::LEFT},
      {Keycode::ARROW_RIGHT, GuiNavCommand::RIGHT},
      {Keycode::KEY_RETURN, GuiNavCommand::CONFIRM},
      {Keycode::SPACE, GuiNavCommand::CONFIRM},
  };

  /// The command @p press's key makes before typing and repeats are
  /// considered.
  std::optional<GuiNavCommand> commandOf(const DesktopNavKey& press) {
    if (press.key == Keycode::ESCAPE) {
      return GuiNavCommand::CANCEL;
    }
    if (press.key == Keycode::TAB) {
      return press.shift ? GuiNavCommand::PREVIOUS : GuiNavCommand::NEXT;
    }
    for (const NavKey& entry : MENU_KEYS) {
      if (!press.typing && entry.key == press.key) {
        return entry.command;
      }
    }
    return std::nullopt;
  }

}  // namespace

std::optional<GuiNavCommand> desktopGuiNavCommand(const DesktopNavKey& press) {
  const std::optional<GuiNavCommand> command = commandOf(press);
  const bool once =
      command == GuiNavCommand::CONFIRM || command == GuiNavCommand::CANCEL;
  return press.repeat && once ? std::nullopt : command;
}

}  // namespace eng::client
