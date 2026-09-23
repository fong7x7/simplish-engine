#pragma once

/// @file mixer-voice.h
/// @brief One sound as the mixer plays it.
/// @par Threading
/// A value type, owned by an `AudioMixer`.

#include <cstdint>
#include <engine/audio/audio-clip.h>
#include <engine/audio/sound-id.h>
#include <engine/audio/sound-play.h>

namespace eng::audio {

/// One voice's state as the mixer plays it.
struct MixerVoice {
  /// Which sound it is playing; none when free.
  SoundId sound{};
  /// The clip, or null when the voice is free.
  const AudioClip* clip = nullptr;
  /// How it plays.
  SoundPlay play{};
  /// Where in the clip it is, in the clip's frames, fraction included.
  double cursor = 0.0;
  /// How far the cursor moves each output frame: the clip's rate over the
  /// output's, times the pitch.
  double step = 1.0;
  /// Its volume on the way out after a stop; one until then.
  float fade = 1.0F;
  /// How much `fade` changes each output frame; negative while stopping.
  float fade_step = 0.0F;
  /// When it started, counted in starts, to find the oldest.
  uint64_t order = 0;
};

}  // namespace eng::audio
