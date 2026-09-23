#pragma once

/// @file sound-play.h
/// @brief Everything a sound needs to start.
/// @par Threading
/// A value type.

#include <cstdint>
#include <engine/audio/audio-bus.h>
#include <engine/audio/audio-clip-id.h>
#include <engine/audio/sound-ducking.h>
#include <engine/audio/sound-loop.h>
#include <engine/audio/sound-placement.h>
#include <engine/math/vec3.h>

namespace eng::audio {

/// Default priority: the middle, so a sound can be ranked either side.
inline constexpr uint8_t SOUND_PRIORITY_DEFAULT = 128;

/// A request to play a clip: which, how loud, where, and how much it
/// matters when every voice is busy.
///
/// When the mixer has no free voice, a new sound takes the voice of the
/// lowest-priority one playing — the oldest of those, if several tie — so
/// long as that one's priority is no higher than its own. Otherwise the new
/// sound is the one dropped. A blast outranks the hundredth shot.
struct SoundPlay {
  /// The clip, in the engine's bank.
  AudioClipId clip{};
  /// Which bus it plays through.
  AudioBus bus = AudioBus::EFFECTS;
  /// Its own volume, 0 and up; one is as recorded.
  float gain = 1.0F;
  /// Playback speed, which is pitch; one is as recorded, two an octave up.
  float pitch = 1.0F;
  /// Whether it ends or loops.
  SoundLoop loop = SoundLoop::ONCE;
  /// Whether it is heard from `at` or from nowhere.
  SoundPlacement placement = SoundPlacement::FLAT;
  /// Where, in tiles, when it is in the world.
  Vec3 at{};
  /// Whether it ducks the music while it plays.
  SoundDucking ducking = SoundDucking::NONE;
  /// How much it matters when voices run out; higher wins.
  uint8_t priority = SOUND_PRIORITY_DEFAULT;
};

}  // namespace eng::audio
