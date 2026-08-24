#pragma once

#include <string_view>

namespace eng {

struct XboxSoundPlayParams {
  /// Path to the audio asset (XMA2 or OGG).
  std::string_view asset_path;
  /// World-space position for spatial audio placement.
  float position[3] = {};
  /// Volume level [0.0, 1.0].
  float volume = 1.0f;
  /// Playback rate multiplier (1.0 = normal speed).
  float pitch = 1.0f;
};

}  // namespace eng
