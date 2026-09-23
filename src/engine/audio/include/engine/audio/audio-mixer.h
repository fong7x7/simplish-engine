#pragma once

/// @file audio-mixer.h
/// @brief Many sounds into one stereo stream.
/// @par Threading
/// Single-threaded: whoever owns it calls every method. `AudioEngine` puts
/// it on the device's thread and talks to it through a queue.

#include <array>
#include <cstdint>
#include <engine/audio/audio-bus.h>
#include <engine/audio/audio-listener.h>
#include <engine/audio/audio-mixer-config.h>
#include <engine/audio/mixer-voice.h>
#include <engine/audio/sound-id.h>
#include <engine/audio/stereo-gain.h>
#include <engine/audio/voice-start.h>
#include <optional>
#include <span>
#include <vector>

namespace eng::audio {

/// Plays up to a fixed number of voices at once into interleaved stereo
/// float frames: each voice read at its own pitch with linear
/// interpolation, placed around the listener if it is in the world, scaled
/// by its bus and the master volume, summed, and clamped.
///
/// Nothing is allocated after construction, and no call takes longer than
/// the voices it walks — it runs where a late buffer is a click.
class AudioMixer {
public:
  /// A mixer with @p config's voices, all free.
  explicit AudioMixer(const AudioMixerConfig& config);

  /// Start @p start on a free voice, or on one stolen from a sound that
  /// matters no more than it (`SoundPlay::priority`), or drop it.
  void start(const VoiceStart& start);

  /// Fade @p sound out over a few milliseconds, so it does not click.
  /// Nothing if it has ended or never started.
  void stop(SoundId sound);

  /// Fade every sound out.
  void stopAll();

  /// @p bus's volume, 0 and up.
  void setBusGain(AudioBus bus, float gain);

  /// The volume over every bus, 0 and up.
  void setMasterGain(float gain);

  /// Where the world is heard from, from the next `mix` on.
  void setListener(const AudioListener& listener);

  /// Write the next `stereo.size() / 2` frames of the mix into @p stereo,
  /// left then right, replacing what it held.
  void mix(std::span<float> stereo);

  /// How many voices are playing.
  [[nodiscard]] uint32_t liveVoices() const;

  /// How many sounds have taken another's voice.
  [[nodiscard]] uint64_t stolenVoices() const { return stolen_; }

  /// How many sounds were dropped: no voice to take, or nothing to play.
  [[nodiscard]] uint64_t droppedSounds() const { return dropped_; }

  /// What the music bus is scaled by for ducking now, duck gain to one.
  [[nodiscard]] float musicDuck() const { return duck_; }

  /// The voices, free ones included, for a test or a debug view.
  [[nodiscard]] std::span<const MixerVoice> voices() const { return voices_; }

private:
  /// A voice for a sound of @p priority: a free one, else the one it
  /// would steal, else none.
  [[nodiscard]] std::optional<size_t> voiceFor(uint8_t priority) const;
  /// The lowest-priority playing voice no higher than @p priority, oldest
  /// first among equals.
  [[nodiscard]] std::optional<size_t> victim(uint8_t priority) const;
  /// Whether any playing voice ducks the music.
  [[nodiscard]] bool ducking() const;
  /// Move the duck toward where it should be, over @p frames.
  void updateDuck(uint32_t frames);
  /// @p voice's gain in each ear, bus, duck and placement included.
  [[nodiscard]] StereoGain voiceGain(const MixerVoice& voice) const;
  /// Add @p voice into @p stereo; false once it has ended.
  bool mixVoice(MixerVoice& voice, std::span<float> stereo) const;

  /// What it was built with.
  AudioMixerConfig config_;
  /// Every voice, free or playing.
  std::vector<MixerVoice> voices_{};
  /// Each bus's volume.
  std::array<float, AUDIO_BUS_COUNT> bus_gains_{1.0F, 1.0F, 1.0F};
  /// The volume over every bus.
  float master_gain_ = 1.0F;
  /// Where the world is heard from.
  AudioListener listener_{};
  /// The music bus's duck, from `duck_gain` to one.
  float duck_ = 1.0F;
  /// Starts so far, which dates each voice.
  uint64_t starts_ = 0;
  /// Voices stolen so far.
  uint64_t stolen_ = 0;
  /// Sounds dropped so far.
  uint64_t dropped_ = 0;
};

}  // namespace eng::audio
