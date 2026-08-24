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
};

}  // namespace eng::client
