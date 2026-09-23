#pragma once

/// @file voice-start.h
/// @brief A sound handed to the mixer to start.
/// @par Threading
/// A value type.

#include <engine/audio/audio-clip.h>
#include <engine/audio/sound-id.h>
#include <engine/audio/sound-play.h>

namespace eng::audio {

/// A `SoundPlay` with its clip found and its id given: what the mixer needs
/// to start a voice without reaching back into the bank.
struct VoiceStart {
  /// The id `AudioEngine::play` gave it.
  SoundId sound{};
  /// The clip, which the bank keeps alive; null starts nothing.
  const AudioClip* clip = nullptr;
  /// How it plays.
  SoundPlay play{};
};

}  // namespace eng::audio
