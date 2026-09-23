#pragma once

/// @file audio-muting.h
/// @brief Whether everything is silenced.
/// @par Threading
/// A value type.

#include <cstdint>

namespace eng::audio {

/// Whether the mix is heard at all — a switch over the volumes, so turning
/// it off brings every level back to where it was.
enum class AudioMuting : uint8_t {
  /// Heard at the volumes set.
  AUDIBLE,
  /// Silent, whatever the volumes say.
  MUTED,
};

}  // namespace eng::audio
