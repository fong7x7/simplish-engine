#pragma once

/// @file audio-mixer-config.h
/// @brief How a mixer is sized and how its ducking moves.
/// @par Threading
/// A value type.

#include <cstdint>

namespace eng::audio {

/// What a mixer is built with.
struct AudioMixerConfig {
  /// The output's frames a second, which the device is opened at.
  uint32_t sample_rate = 48000;
  /// How many sounds play at once; past that, voices are stolen.
  uint16_t voices = 48;
  /// The music bus's gain while a ducking sound plays.
  float duck_gain = 0.35F;
  /// How long the music takes to go down, in seconds.
  float duck_attack_seconds = 0.04F;
  /// How long it takes to come back up, in seconds.
  float duck_release_seconds = 0.8F;
};

}  // namespace eng::audio
