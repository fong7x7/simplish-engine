#pragma once

/// @file gamepad-reading.h
/// @brief One connected pad's state, as a backend read it this frame.
/// @par Threading
/// A value type.

#include <cstdint>
#include <engine/input/gamepad-family.h>
#include <engine/input/gamepad-state.h>

namespace eng::input {

/// What a platform pad backend reports for one pad each frame: which pad,
/// by an id the backend keeps stable for as long as it is connected, and
/// what its controls read.
struct GamepadReading {
  /// The backend's id for the pad; only ever compared.
  uint64_t device = 0;
  /// Its buttons and axes, raw.
  GamepadState state;
  /// Whose layout it follows, for prompts and the confirm button.
  GamepadFamily family = GamepadFamily::GENERIC;
};

}  // namespace eng::input
