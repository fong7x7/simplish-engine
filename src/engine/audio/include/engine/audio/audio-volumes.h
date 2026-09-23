#pragma once

/// @file audio-volumes.h
/// @brief How loud a player wants each part of the mix.
/// @par Threading
/// A value type, and main-thread calls into an `AudioEngine`.

#include <array>
#include <engine/audio/audio-bus.h>
#include <engine/audio/audio-engine.h>
#include <engine/audio/audio-muting.h>

namespace eng::audio {

/// The volume settings a player chooses: one for everything, one for each
/// bus, and a mute. Each volume is a slider's position, 0 to 1 — not a gain:
/// `audioVolumeGain` turns it into one along a curve the ear hears as even,
/// so halfway sounds like half.
struct AudioVolumes {
  /// The volume over every bus, 0 to 1.
  float master = 1.0F;
  /// Each bus's volume, 0 to 1, by `AudioBus`.
  std::array<float, AUDIO_BUS_COUNT> buses{1.0F, 1.0F, 1.0F};
  /// Whether everything is silenced.
  AudioMuting muting = AudioMuting::AUDIBLE;
};

/// The gain a slider at @p volume gives: its square, after clamping to 0
/// to 1. Loudness is heard on a log scale; a square follows it closely
/// enough from full down to about -40 dB, and reaches true silence at zero,
/// which a decibel curve never does.
[[nodiscard]] float audioVolumeGain(float volume);

/// @p volumes' setting for @p bus.
[[nodiscard]] float busVolume(const AudioVolumes& volumes, AudioBus bus);

/// Set @p bus's volume in @p volumes to @p volume, clamped to 0 to 1.
void setBusVolume(AudioVolumes& volumes, AudioBus bus, float volume);

/// Send @p volumes to @p engine: the master gain (zero while muted), and
/// each bus's.
void applyAudioVolumes(AudioEngine& engine, const AudioVolumes& volumes);

}  // namespace eng::audio
