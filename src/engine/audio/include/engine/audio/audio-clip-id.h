#pragma once

/// @file audio-clip-id.h
/// @brief Which clip in a bank a sound plays.
/// @par Threading
/// A value type.

#include <cstdint>

namespace eng::audio {

/// A clip's place in its `AudioClipBank`: the order it was added in. Clips
/// are never taken out of a bank, so an id stays good for the bank's life.
struct AudioClipId {
  /// Index into the bank.
  uint32_t index = 0;

  /// Whether @p other names the same clip.
  bool operator==(const AudioClipId& other) const = default;
};

}  // namespace eng::audio
