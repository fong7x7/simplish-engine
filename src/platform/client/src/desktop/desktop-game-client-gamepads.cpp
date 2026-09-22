// The pad and menu-navigation half of DesktopGameClient: opening the
// platform's pad backend, reading it once a frame, keeping it quiet while
// the window is not the one being used, and — when a menu asks — turning
// the pad in use and the navigation keys into GUI focus commands. Which
// pads exist is platform/input's business; this owns the backend's
// lifetime and the hooks.

#include "engine/client/desktop-game-client.h"

#include <SDL3/SDL.h>
#include <engine/client/desktop-gui-nav-keys.h>
#include <engine/core/logger.h>

namespace eng::client {

void DesktopGameClient::openGamepads() {
  if (const std::optional<std::string> error = gamepads_.open()) {
    LOG_WARN("client", "No gamepads: " + *error);
  }
}

void DesktopGameClient::pollGamepads(float dt_seconds) {
  gamepads_.poll();
  for (std::size_t i = 0; i < eng::input::GAMEPAD_BUTTON_COUNT; ++i) {
    const auto button = static_cast<eng::input::GamepadButton>(i);
    if (gamepads_.pads().pressed(button)) {
      onClientGamepadButtonDown(button);
    }
  }
  if (gui_pad_navigation_ == GuiPadNavigation::ON) {
    navigateGuiByPad(dt_seconds);
  }
}

void DesktopGameClient::navigateGuiByPad(float dt_seconds) {
  const eng::input::GamepadSet& pads = gamepads_.pads();
  for (const eng::GuiNavCommand command :
       gui_navigator_.update(pads.active(), pads.activeFamily(), dt_seconds)) {
    dispatchNav(command);
  }
}

void DesktopGameClient::dispatchNav(eng::GuiNavCommand command) {
  if (!guiDispatchNav(command)) {
    onClientGuiNavUnhandled(command);
  }
}

bool DesktopGameClient::navigateGuiByKey(const SDL_Event& event) {
  if (gui_key_navigation_ == GuiKeyNavigation::OFF) {
    return false;
  }
  const DesktopNavKey press{static_cast<uint32_t>(event.key.key),
                            (event.key.mod & SDL_KMOD_SHIFT) != 0U,
                            guiWidgetTree().hasFocusedInput(),
                            event.key.repeat};
  const std::optional<eng::GuiNavCommand> command = desktopGuiNavCommand(press);
  if (command) {
    dispatchNav(*command);
  }
  return command.has_value();
}

void DesktopGameClient::setGuiPadNavigation(GuiPadNavigation mode) {
  gui_pad_navigation_ = mode;
  gui_navigator_.reset(gamepads_.pads().active());
}

void DesktopGameClient::trackGamepadFocus(const SDL_Event& event) {
  if (event.type == SDL_EVENT_WINDOW_FOCUS_LOST) {
    gamepads_.setFocus(eng::input::WindowFocus::UNFOCUSED);
  } else if (event.type == SDL_EVENT_WINDOW_FOCUS_GAINED) {
    gamepads_.setFocus(eng::input::WindowFocus::FOCUSED);
  }
}

}  // namespace eng::client
