#pragma once

/// @file audio-engine.h
/// @brief Playing sounds from the game, mixed on the device's thread.
/// @par Threading
/// `render` runs on the audio device's thread; everything else is
/// main-thread-only. The two meet in a lock-free queue and a few atomics.

#include <atomic>
#include <cstdint>
#include <engine/audio/audio-bus.h>
#include <engine/audio/audio-clip-bank.h>
#include <engine/audio/audio-command-queue.h>
#include <engine/audio/audio-listener.h>
#include <engine/audio/audio-mixer-config.h>
#include <engine/audio/audio-mixer.h>
#include <engine/audio/sound-id.h>
#include <engine/audio/sound-play.h>
#include <span>

namespace eng::audio {

/// How many requests may wait for the mixer between two of its buffers.
inline constexpr uint32_t AUDIO_COMMAND_CAPACITY = 512;

/// The engine's sound: a bank of clips, and a mixer the main thread plays
/// them through. It knows nothing of any device. The platform's
/// `AudioDevice` calls `render` from its own thread whenever the hardware
/// wants samples, and a test calls it by hand to hear what was played.
///
/// Presentation only (ADR-002): the simulation never calls this, never
/// reads it back, and nothing here is hashed. Game code turns a tick's cues
/// into `SoundPlay`s after the tick, as it does effects.
class AudioEngine {
public:
  /// An engine mixing at @p config's rate, with its voices.
  explicit AudioEngine(const AudioMixerConfig& config = {});
  AudioEngine(const AudioEngine&) = delete;
  AudioEngine& operator=(const AudioEngine&) = delete;
  AudioEngine(AudioEngine&&) = delete;
  AudioEngine& operator=(AudioEngine&&) = delete;
  ~AudioEngine() = default;

  /// The clips sounds are played from.
  [[nodiscard]] AudioClipBank& clips() { return clips_; }

  /// The clips sounds are played from.
  [[nodiscard]] const AudioClipBank& clips() const { return clips_; }

  /// Start @p play, and give the id it can be stopped by; none if its clip
  /// is not in the bank or too many requests are waiting.
  SoundId play(const SoundPlay& play);

  /// Fade @p sound out.
  void stop(SoundId sound);

  /// Fade every sound out.
  void stopAll();

  /// @p bus's volume, 0 and up.
  void setBusGain(AudioBus bus, float gain);

  /// The volume over every bus, 0 and up.
  void setMasterGain(float gain);

  /// Where the world is heard from.
  void setListener(const AudioListener& listener);

  /// On the device's thread: apply what the main thread asked since last
  /// time, then write the next `stereo.size() / 2` frames into @p stereo.
  void render(std::span<float> stereo);

  /// The frames a second `render` writes, which a device opens at.
  [[nodiscard]] uint32_t sampleRate() const { return sample_rate_; }

  /// Voices playing as of the last `render`.
  [[nodiscard]] uint32_t liveVoices() const;

  /// Frames written by `render` so far.
  [[nodiscard]] uint64_t framesRendered() const;

  /// Sounds `play` has sent to the mixer.
  [[nodiscard]] uint64_t soundsPlayed() const { return played_; }

  /// Sounds that never played: refused by `play`, or dropped by the mixer
  /// for want of a voice, as of the last `render`.
  [[nodiscard]] uint64_t soundsDropped() const;

private:
  /// Send @p command to the mixer; false if the queue is full.
  bool send(const AudioCommand& command);
  /// Apply one command to the mixer.
  void apply(const AudioCommand& command);

  /// The clips.
  AudioClipBank clips_{};
  /// Requests on their way to the mixer.
  AudioCommandQueue queue_{AUDIO_COMMAND_CAPACITY};
  /// The mixer, touched only by `render`.
  AudioMixer mixer_;
  /// The output rate.
  uint32_t sample_rate_ = 0;
  /// The next id `play` gives.
  uint32_t next_sound_ = 1;
  /// Sounds sent.
  uint64_t played_ = 0;
  /// Sounds `play` refused.
  uint64_t refused_ = 0;
  /// Voices playing, published by `render`.
  std::atomic<uint32_t> live_{0};
  /// Frames written, published by `render`.
  std::atomic<uint64_t> frames_{0};
  /// Sounds the mixer dropped, published by `render`.
  std::atomic<uint64_t> mixer_dropped_{0};
};

}  // namespace eng::audio
