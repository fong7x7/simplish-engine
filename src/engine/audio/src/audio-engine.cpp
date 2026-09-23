#include <array>
#include <engine/audio/audio-engine.h>

namespace eng::audio {

namespace {

  /// What a command of one kind does to the mixer.
  using ApplyCommand = void (*)(AudioMixer&, const AudioCommand&);

  /// Each kind's, by `AudioCommandKind`.
  constexpr std::array<ApplyCommand, AUDIO_COMMAND_KIND_COUNT> APPLY{
      [](AudioMixer& m, const AudioCommand& c) { m.start(c.start); },
      [](AudioMixer& m, const AudioCommand& c) { m.stop(c.start.sound); },
      [](AudioMixer& m, const AudioCommand&) { m.stopAll(); },
      [](AudioMixer& m, const AudioCommand& c) { m.setBusGain(c.bus, c.gain); },
      [](AudioMixer& m, const AudioCommand& c) { m.setMasterGain(c.gain); },
      [](AudioMixer& m, const AudioCommand& c) { m.setListener(c.listener); },
  };
  static_assert(static_cast<size_t>(AudioCommandKind::LISTENER) + 1 ==
                AUDIO_COMMAND_KIND_COUNT);

}  // namespace

AudioEngine::AudioEngine(const AudioMixerConfig& config)
  : mixer_(config), sample_rate_(config.sample_rate) {}

SoundId AudioEngine::play(const SoundPlay& play) {
  const AudioClip* clip = clips_.clip(play.clip);
  const SoundId sound{next_sound_};
  if (clip == nullptr ||
      !send({.kind = AudioCommandKind::START,
             .start = {.sound = sound, .clip = clip, .play = play}})) {
    ++refused_;
    return {};
  }
  ++next_sound_;
  ++played_;
  return sound;
}

void AudioEngine::stop(SoundId sound) {
  (void)send({.kind = AudioCommandKind::STOP, .start = {.sound = sound}});
}

void AudioEngine::stopAll() {
  (void)send({.kind = AudioCommandKind::STOP_ALL});
}

void AudioEngine::setBusGain(AudioBus bus, float gain) {
  (void)send({.kind = AudioCommandKind::BUS_GAIN, .bus = bus, .gain = gain});
}

void AudioEngine::setMasterGain(float gain) {
  (void)send({.kind = AudioCommandKind::MASTER_GAIN, .gain = gain});
}

void AudioEngine::setListener(const AudioListener& listener) {
  (void)send({.kind = AudioCommandKind::LISTENER, .listener = listener});
}

bool AudioEngine::send(const AudioCommand& command) {
  return queue_.push(command);
}

void AudioEngine::render(std::span<float> stereo) {
  while (const std::optional<AudioCommand> command = queue_.pop()) {
    apply(*command);
  }
  mixer_.mix(stereo);
  live_.store(mixer_.liveVoices(), std::memory_order_relaxed);
  mixer_dropped_.store(mixer_.droppedSounds(), std::memory_order_relaxed);
  frames_.fetch_add(stereo.size() / 2, std::memory_order_relaxed);
}

void AudioEngine::apply(const AudioCommand& command) {
  APPLY[static_cast<size_t>(command.kind)](mixer_, command);
}

uint32_t AudioEngine::liveVoices() const {
  return live_.load(std::memory_order_relaxed);
}

uint64_t AudioEngine::framesRendered() const {
  return frames_.load(std::memory_order_relaxed);
}

uint64_t AudioEngine::soundsDropped() const {
  return refused_ + mixer_dropped_.load(std::memory_order_relaxed);
}

}  // namespace eng::audio
