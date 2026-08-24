#pragma once

#include "xbox-audio-config.h"

#include <cstdint>

namespace eng {

/// Audio context for XAudio2 spatial backend. Actual XAudio2 and
/// ISpatialAudioClient pointers stored internally in .cpp.
struct XboxAudioContext {
  /// True after XAudio2 and spatial audio have been initialised.
  bool initialised = false;
  /// Configured maximum number of spatial audio objects.
  uint32_t max_spatial_objects = XBOX_MAX_SPATIAL_AUDIO_OBJECTS;
  /// Number of spatial audio objects currently in use.
  uint32_t active_spatial_objects = 0;
};

}  // namespace eng
