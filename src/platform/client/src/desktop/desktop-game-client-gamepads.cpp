// The pad half of DesktopGameClient: opening the platform's pad backend,
// reading it once a frame, and keeping it quiet while the window is not
// the one being used. Which pads exist is platform/input's business; this
// only owns the backend's lifetime and turns presses into a hook.

#include "engine/client/desktop-game-client.h"

#include <SDL3/SDL.h>
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
  for (const eng::GuiNavCommand command :
       gui_navigator_.update(gamepads_.pads().active(), dt_seconds)) {
    if (!guiDispatchNav(command)) {
      onClientGuiNavUnhandled(command);
    }
  }
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
