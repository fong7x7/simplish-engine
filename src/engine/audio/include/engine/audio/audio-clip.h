#pragma once

/// @file audio-clip.h
/// @brief A sound, decoded: samples a mixer can play.
/// @par Threading
/// A value type. Once a clip is in an `AudioClipBank` it is never written
/// again, so the mixer's thread reads it while the main thread goes on.

#include <cstdint>
#include <vector>

namespace eng::audio {

/// Decoded audio, as 32-bit float samples from -1 to 1, interleaved when
/// there are two channels. Mono or stereo only: a mono clip is placed in the
/// world by the mixer, a stereo one plays as it was mixed.
///
/// A clip keeps the sample rate it was recorded at. The mixer steps through
/// it at that rate over its own, so nothing is resampled when it loads.
struct AudioClip {
  /// The samples, frame by frame; `channels` of them a frame.
  std::vector<float> samples{};
  /// Frames a second.
  uint32_t sample_rate = 48000;
  /// One or two.
  uint8_t channels = 1;
};

/// How many frames @p clip holds: a sample for each channel is one frame.
[[nodiscard]] uint32_t clipFrames(const AudioClip& clip);

/// How long @p clip plays for at its own rate, in seconds.
[[nodiscard]] float clipSeconds(const AudioClip& clip);

}  // namespace eng::audio
