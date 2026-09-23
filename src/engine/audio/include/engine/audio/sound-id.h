#pragma once

/// @file sound-id.h
/// @brief One playing of a sound, to stop or ask after.
/// @par Threading
/// A value type.

#include <cstdint>

namespace eng::audio {

/// Names one call to `AudioEngine::play`, not the clip it played: the same
/// clip played twice is two sounds, stopped apart. Ids count up from one and
/// are never reused within an engine's life; zero is no sound, which is what
/// a play that could not start returns.
struct SoundId {
  /// The count; zero is none.
  uint32_t value = 0;

  /// Whether @p other names the same playing.
  bool operator==(const SoundId& other) const = default;
};

}  // namespace eng::audio
