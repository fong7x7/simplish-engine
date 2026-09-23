#pragma once

/// @file sound-loop.h
/// @brief Whether a sound ends or goes round again.
/// @par Threading
/// A value type.

#include <cstdint>

namespace eng::audio {

/// What a sound does when it reaches the end of its clip.
enum class SoundLoop : uint8_t {
  /// Stops, and gives its voice back.
  ONCE,
  /// Starts again from the top, until it is stopped.
  LOOP,
};

}  // namespace eng::audio
