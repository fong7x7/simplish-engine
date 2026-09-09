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
};

}  // namespace eng::client
