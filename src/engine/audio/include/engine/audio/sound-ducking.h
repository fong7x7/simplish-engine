#pragma once

/// @file sound-ducking.h
/// @brief Whether a sound pulls the music down under it.
/// @par Threading
/// A value type.

#include <cstdint>

namespace eng::audio {

/// Whether a sound ducks the music bus while it plays — a blast that should
/// be heard over the score rather than through it.
enum class SoundDucking : uint8_t {
  /// Leaves the music alone.
  NONE,
  /// Holds the music at the mixer's duck gain until it ends.
  DUCKS_MUSIC,
};

}  // namespace eng::audio
