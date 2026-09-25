#pragma once

/// @file ui-screen-layer.h
/// @brief Whether a game screen takes input.
/// @par Threading
/// A value type.

#include <cstdint>

namespace eng::game {

/// What kind of screen it is.
enum class UiScreenLayer : uint8_t {
  /// Modal: drawn over a dimmed game, above every HUD, and the pad and
  /// keyboard move between its buttons.
  MENU,
  /// Drawn over play, never taking input: a score, health, a timer.
  HUD,
};

}  // namespace eng::game
