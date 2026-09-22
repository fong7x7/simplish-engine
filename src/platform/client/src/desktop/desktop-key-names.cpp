#include <SDL3/SDL.h>
#include <engine/client/desktop-key-names.h>

namespace eng::client {

namespace {

  /// Every named key, by SDL's own symbol so the two cannot drift.
  constexpr eng::input::KeyName KEY_NAMES[] = {
      {"up", SDLK_UP},
      {"down", SDLK_DOWN},
      {"left", SDLK_LEFT},
      {"right", SDLK_RIGHT},
      {"space", SDLK_SPACE},
      {"return", SDLK_RETURN},
      {"escape", SDLK_ESCAPE},
      {"tab", SDLK_TAB},
      {"backspace", SDLK_BACKSPACE},
      {"delete", SDLK_DELETE},
      {"insert", SDLK_INSERT},
      {"home", SDLK_HOME},
      {"end", SDLK_END},
      {"pageup", SDLK_PAGEUP},
      {"pagedown", SDLK_PAGEDOWN},
      {"lshift", SDLK_LSHIFT},
      {"rshift", SDLK_RSHIFT},
      {"lctrl", SDLK_LCTRL},
      {"rctrl", SDLK_RCTRL},
      {"lalt", SDLK_LALT},
      {"ralt", SDLK_RALT},
      {"lgui", SDLK_LGUI},
      {"rgui", SDLK_RGUI},
      {"capslock", SDLK_CAPSLOCK},
      {"f1", SDLK_F1},
      {"f2", SDLK_F2},
      {"f3", SDLK_F3},
      {"f4", SDLK_F4},
      {"f5", SDLK_F5},
      {"f6", SDLK_F6},
      {"f7", SDLK_F7},
      {"f8", SDLK_F8},
      {"f9", SDLK_F9},
      {"f10", SDLK_F10},
      {"f11", SDLK_F11},
      {"f12", SDLK_F12},
  };

}  // namespace

std::span<const eng::input::KeyName> desktopKeyNames() {
  return KEY_NAMES;
}

}  // namespace eng::client
