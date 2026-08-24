#pragma once

#include <cstdint>

namespace eng {

constexpr uint32_t XBOX_MAX_SPATIAL_AUDIO_OBJECTS = 128;

struct XboxAudioConfig {
  /// Maximum number of simultaneous spatial audio objects.
  uint32_t max_spatial_objects = XBOX_MAX_SPATIAL_AUDIO_OBJECTS;
};

}  // namespace eng
