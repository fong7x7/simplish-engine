#pragma once

/// @file stereo-gain.h
/// @brief How loud a sound is in each ear.
/// @par Threading
/// A value type.

namespace eng::audio {

/// A gain for each channel of the output, 0 and up; one is as recorded.
struct StereoGain {
  /// The left channel's.
  float left = 1.0F;
  /// The right channel's.
  float right = 1.0F;
};

}  // namespace eng::audio
