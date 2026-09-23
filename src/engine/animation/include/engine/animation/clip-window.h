#pragma once

/// @file clip-window.h
/// @brief The stretch of a clip one frame played.
/// @par Threading
/// A value type.

namespace eng::animation {

/// Seconds since a looping clip started, from where the last frame left
/// its playhead to where this one puts it. Unwrapped: a window may run past
/// the clip's end and on into its next loop.
struct ClipWindow {
  /// Where the playhead was, exclusive.
  double from = 0.0;
  /// Where it is now, inclusive.
  double to = 0.0;
};

}  // namespace eng::animation
