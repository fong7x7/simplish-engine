#pragma once

#include <cstdint>

namespace eng::client {

/// SDL3 `SDL_Keycode` values for `onClientKeyDown` comparisons without SDL
/// headers.  Verified against SDL in `desktop-game-client.cpp`.
struct DesktopPlatformKeycode {
  /// Matches `SDLK_ESCAPE`.
  static constexpr uint32_t ESCAPE = 27U;
  /// Matches `SDLK_RETURN` (main keyboard Enter).
  static constexpr uint32_t KEY_RETURN = 13U;
  /// Matches `SDLK_BACKSPACE` — the key labelled "delete" on Mac layouts.
  static constexpr uint32_t BACKSPACE = 8U;
  /// Matches `SDLK_DELETE`, the forward delete of a full-size keyboard.
  ///
  /// Not `DELETE`: `<windows.h>` defines that as an access mask, and a
  /// macro is not scoped away by the struct this sits in.
  static constexpr uint32_t DELETE_FORWARD = 127U;
  /// Matches `SDLK_F5`: start and stop a playtest, as most editors bind it.
  static constexpr uint32_t F5 = 0x4000003EU;
  /// Matches `SDLK_RIGHT`.
  static constexpr uint32_t ARROW_RIGHT = 0x4000004FU;
  /// Matches `SDLK_LEFT`.
  static constexpr uint32_t ARROW_LEFT = 0x40000050U;
  /// Matches `SDLK_DOWN`.
  static constexpr uint32_t ARROW_DOWN = 0x40000051U;
  /// Matches `SDLK_UP`.
  static constexpr uint32_t ARROW_UP = 0x40000052U;
};

}  // namespace eng::client
