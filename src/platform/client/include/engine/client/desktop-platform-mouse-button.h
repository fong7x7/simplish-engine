#pragma once

/// @file desktop-platform-mouse-button.h
/// @brief SDL3 mouse button indices and their mapping to `GuiMouseButton`.
/// @par Threading Main thread only.

#include <cstdint>
#include <engine/gui/gui-mouse-event.h>

namespace eng::client {

/// SDL3 `SDL_BUTTON_*` values, mirrored so this mapping can be tested
/// without SDL headers. Verified against SDL in `desktop-game-client.cpp`.
struct DesktopPlatformMouseButton {
  /// Matches `SDL_BUTTON_LEFT`.
  static constexpr uint8_t LEFT = 1U;
  /// Matches `SDL_BUTTON_MIDDLE`.
  static constexpr uint8_t MIDDLE = 2U;
  /// Matches `SDL_BUTTON_RIGHT`.
  static constexpr uint8_t RIGHT = 3U;
};

/// Map an SDL button index to the GUI's button enum.
///
/// Anything unrecognised — the extra buttons on a gaming mouse — reports as
/// LEFT, which is the button every widget already handles.
[[nodiscard]] constexpr eng::GuiMouseButton
mapDesktopMouseButton(uint8_t sdl_button) {
  switch (sdl_button) {
    case DesktopPlatformMouseButton::MIDDLE:
      return eng::GuiMouseButton::MIDDLE;
    case DesktopPlatformMouseButton::RIGHT:
      return eng::GuiMouseButton::RIGHT;
    default:
      return eng::GuiMouseButton::LEFT;
  }
}

}  // namespace eng::client
