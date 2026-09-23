#include <algorithm>
#include <cmath>
#include <engine/audio/audio-mixer.h>
#include <engine/audio/audio-spatial.h>

namespace eng::audio {

namespace {

  /// How long a stopped sound takes to fade out, in seconds: long enough
  /// not to click, short enough to read as a stop.
  constexpr float STOP_FADE_SECONDS = 0.01F;
  /// The slowest a voice may play, so a pitch of zero still ends.
  constexpr double MIN_PITCH = 0.01;

  /// Frame @p index of @p clip, left and right; a mono clip in both.
  StereoGain frameAt(const AudioClip& clip, uint32_t index) {
    const size_t at = static_cast<size_t>(index) * clip.channels;
    const float left = clip.samples[at];
    return {.left = left,
            .right = clip.channels == 2 ? clip.samples[at + 1] : left};
  }

  /// @p clip at @p cursor, between its two nearest frames; past the last,
  /// toward the first again if it loops and holding if not.
  StereoGain readFrame(const AudioClip& clip, double cursor, SoundLoop loop) {
    const uint32_t frames = clipFrames(clip);
    const auto index = static_cast<uint32_t>(cursor);
    uint32_t next = index + 1;
    if (next >= frames) {
      next = loop == SoundLoop::LOOP ? 0 : index;
    }
    const auto along = static_cast<float>(cursor - index);
    const StereoGain a = frameAt(clip, index);
    const StereoGain b = frameAt(clip, next);
    return {.left = std::lerp(a.left, b.left, along),
            .right = std::lerp(a.right, b.right, along)};
  }

  /// Bring @p voice's cursor back inside its clip; false once a sound that
  /// does not loop has played through.
  bool wrapCursor(MixerVoice& voice) {
    const auto end = static_cast<double>(clipFrames(*voice.clip));
    if (voice.cursor < end) {
      return true;
    }
    if (voice.play.loop == SoundLoop::ONCE) {
      return false;
    }
    voice.cursor = std::fmod(voice.cursor, end);
    return true;
  }

}  // namespace

AudioMixer::AudioMixer(const AudioMixerConfig& config)
  : config_(config), voices_(std::max<uint16_t>(config.voices, 1)) {
  config_.sample_rate = std::max(config_.sample_rate, 1U);
}

void AudioMixer::start(const VoiceStart& start) {
  const bool playable = start.clip != nullptr && clipFrames(*start.clip) > 0;
  const std::optional<size_t> slot =
      playable ? voiceFor(start.play.priority) : std::nullopt;
  if (!slot) {
    ++dropped_;
    return;
  }
  stolen_ += voices_[*slot].clip != nullptr ? 1 : 0;
  const double pitch = std::max<double>(start.play.pitch, MIN_PITCH);
  voices_[*slot] =
      MixerVoice{.sound = start.sound,
                 .clip = start.clip,
                 .play = start.play,
                 .step = pitch * start.clip->sample_rate / config_.sample_rate,
                 .order = ++starts_};
}

std::optional<size_t> AudioMixer::voiceFor(uint8_t priority) const {
  for (size_t i = 0; i < voices_.size(); ++i) {
    if (voices_[i].clip == nullptr) {
      return i;
    }
  }
  return victim(priority);
}

std::optional<size_t> AudioMixer::victim(uint8_t priority) const {
  std::optional<size_t> found;
  for (size_t i = 0; i < voices_.size(); ++i) {
    const MixerVoice& voice = voices_[i];
    if (voice.play.priority > priority) {
      continue;
    }
    const MixerVoice* best = found ? &voices_[*found] : nullptr;
    if (best == nullptr || voice.play.priority < best->play.priority ||
        (voice.play.priority == best->play.priority &&
         voice.order < best->order)) {
      found = i;
    }
  }
  return found;
}

void AudioMixer::stop(SoundId sound) {
  const float step =
      -1.0F / (STOP_FADE_SECONDS * static_cast<float>(config_.sample_rate));
  for (MixerVoice& voice : voices_) {
    if (voice.clip != nullptr && voice.sound == sound) {
      voice.fade_step = step;
    }
  }
}

void AudioMixer::stopAll() {
  for (const MixerVoice& voice : voices_) {
    if (voice.clip != nullptr) {
      stop(voice.sound);
    }
  }
}

void AudioMixer::setBusGain(AudioBus bus, float gain) {
  bus_gains_[static_cast<size_t>(bus)] = std::max(gain, 0.0F);
}

void AudioMixer::setMasterGain(float gain) {
  master_gain_ = std::max(gain, 0.0F);
}

void AudioMixer::setListener(const AudioListener& listener) {
  listener_ = listener;
}

void AudioMixer::mix(std::span<float> stereo) {
  std::ranges::fill(stereo, 0.0F);
  updateDuck(static_cast<uint32_t>(stereo.size() / 2));
  for (MixerVoice& voice : voices_) {
    if (voice.clip != nullptr && !mixVoice(voice, stereo)) {
      voice = MixerVoice{};
    }
  }
  for (float& sample : stereo) {
    sample = std::clamp(sample * master_gain_, -1.0F, 1.0F);
  }
}

uint32_t AudioMixer::liveVoices() const {
  return static_cast<uint32_t>(std::ranges::count_if(
      voices_, [](const MixerVoice& v) { return v.clip != nullptr; }));
}

bool AudioMixer::ducking() const {
  return std::ranges::any_of(voices_, [](const MixerVoice& v) {
    return v.clip != nullptr && v.fade_step >= 0.0F &&
           v.play.ducking == SoundDucking::DUCKS_MUSIC;
  });
}

void AudioMixer::updateDuck(uint32_t frames) {
  const float target = ducking() ? config_.duck_gain : 1.0F;
  const float seconds =
      static_cast<float>(frames) / static_cast<float>(config_.sample_rate);
  const float time = target < duck_ ? config_.duck_attack_seconds
                                    : config_.duck_release_seconds;
  const float moved = 1.0F - std::exp(-seconds / std::max(time, 1e-4F));
  duck_ += (target - duck_) * moved;
}

StereoGain AudioMixer::voiceGain(const MixerVoice& voice) const {
  const AudioBus bus = voice.play.bus;
  float gain = voice.play.gain * bus_gains_[static_cast<size_t>(bus)];
  if (bus == AudioBus::MUSIC) {
    gain *= duck_;
  }
  const StereoGain placed = voice.play.placement == SoundPlacement::IN_WORLD
                                ? spatialGain(listener_, voice.play.at)
                                : StereoGain{};
  return {.left = placed.left * gain, .right = placed.right * gain};
}

bool AudioMixer::mixVoice(MixerVoice& voice, std::span<float> stereo) const {
  const StereoGain gain = voiceGain(voice);
  for (size_t i = 0; i + 1 < stereo.size(); i += 2) {
    if (!wrapCursor(voice)) {
      return false;
    }
    const StereoGain frame =
        readFrame(*voice.clip, voice.cursor, voice.play.loop);
    stereo[i] += frame.left * gain.left * voice.fade;
    stereo[i + 1] += frame.right * gain.right * voice.fade;
    voice.cursor += voice.step;
    voice.fade += voice.fade_step;
    if (voice.fade <= 0.0F) {
      return false;
    }
  }
  return true;
}

}  // namespace eng::audio
