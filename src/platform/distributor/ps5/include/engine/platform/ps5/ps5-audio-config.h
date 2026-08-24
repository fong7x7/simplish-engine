#pragma once

#include "ps5-types.h"

#include <cstdint>

namespace eng {

/// Configuration for the Tempest audio backend.
struct Ps5AudioConfig {
  /// Maximum number of simultaneous Tempest audio sources.
  uint32_t max_sources = PS5_TEMPEST_MAX_SOURCES;
  /// Audio output sample rate in Hz.
  uint32_t sample_rate = PS5_TEMPEST_DEFAULT_SAMPLE_RATE_HZ;
  /// Preferred audio codec for asset decoding.
  Ps5AudioCodecPreference codec_preference =
      Ps5AudioCodecPreference::ATRAC9_PREFERRED;
};

}  // namespace eng
