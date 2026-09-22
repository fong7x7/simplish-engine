#pragma once

/// @file gamepad-deadzones.h
/// @brief How far a stick or trigger must move before it counts.
/// @par Threading
/// A value type.

namespace eng::input {

/// The travel, as a fraction of full scale, that a pad ignores: sticks
/// rest a little off centre and triggers a little off zero, and without
/// these a pad on the desk walks the player slowly away.
///
/// A stick's is radial — measured on the stick's length, not per axis —
/// so a push at any angle starts at the same distance and the diagonals
/// are not dead. Travel past it is rescaled to start from zero, so the
/// slowest move is slow rather than a jump to a fifth of full speed.
struct GamepadDeadzones {
  /// The left stick's, 0 to under 1.
  float left_stick = 0.2F;
  /// The right stick's, 0 to under 1.
  float right_stick = 0.2F;
  /// Both triggers', 0 to under 1.
  float trigger = 0.1F;
};

}  // namespace eng::input
