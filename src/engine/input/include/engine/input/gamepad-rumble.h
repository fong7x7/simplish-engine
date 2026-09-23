#pragma once

/// @file gamepad-rumble.h
/// @brief A burst of vibration to play on a pad.
/// @par Threading
/// A value type, and pure functions over it.

namespace eng::input {

/// How hard a pad's motors run, each 0 to 1, and for how long. Device-
/// neutral: the platform's pad backend plays it on whatever the pad has —
/// the two body motors every pad since the DualShock carries, and the
/// trigger motors of an Xbox One or Series pad — and ignores what it lacks.
///
/// Presentation, like a spark: made from what a tick did, never read back.
struct GamepadRumble {
  /// The heavy, low-frequency motor — a blast, a hit taken.
  float low = 0.0F;
  /// The light, high-frequency motor — a shot's kick.
  float high = 0.0F;
  /// The left trigger's motor, where the pad has one.
  float left_trigger = 0.0F;
  /// The right trigger's motor, where the pad has one.
  float right_trigger = 0.0F;
  /// How long it plays, in seconds; 0 is nothing.
  float seconds = 0.0F;
};

/// Whether @p rumble would do anything.
[[nodiscard]] bool isRumbling(const GamepadRumble& rumble);

/// @p a and @p b played together: each motor at the stronger of the two,
/// for the longer of the two times. Several things at once feel like the
/// biggest of them, not their sum, which would pin every motor.
[[nodiscard]] GamepadRumble strongerRumble(const GamepadRumble& a,
                                           const GamepadRumble& b);

}  // namespace eng::input
