#include <algorithm>
#include <cmath>
#include <engine/audio/audio-synth.h>
#include <numbers>

namespace eng::audio {

namespace {

  /// How long the last moment of a clip takes to fade to silence, so it
  /// never ends on a click.
  constexpr float TAIL_SECONDS = 0.004F;
  /// The loudest sample a synthesised clip is scaled to.
  constexpr float PEAK = 0.9F;

  /// What carries over from one sample to the next.
  struct SynthState {
    /// The tone's phase, in radians.
    float phase = 0.0F;
    /// The low-passed noise's last value.
    float filtered = 0.0F;
    /// The noise generator's state; never zero.
    uint32_t noise = 1;
  };

  /// The next white-noise sample, -1 to 1, from @p state (xorshift32).
  float nextNoise(uint32_t& state) {
    state ^= state << 13U;
    state ^= state >> 17U;
    state ^= state << 5U;
    return static_cast<float>(state) / 2147483648.0F - 1.0F;
  }

  /// The volume envelope at @p t seconds in.
  float envelope(const SynthSpec& spec, float t) {
    const float attack = std::max(spec.attack_seconds, 1e-5F);
    const float decay = std::max(spec.decay_seconds, 1e-5F);
    const float rise = std::min(t / attack, 1.0F);
    const float fall = t > attack ? std::exp(-(t - attack) / decay) : 1.0F;
    const float tail =
        std::clamp((spec.seconds - t) / TAIL_SECONDS, 0.0F, 1.0F);
    return rise * fall * tail;
  }

  /// The sample at @p t seconds in, moving @p state on.
  float synthSample(const SynthSpec& spec, SynthState& state, float t,
                    float rate) {
    const float along =
        std::clamp(t / std::max(spec.seconds, 1e-5F), 0.0F, 1.0F);
    const float tone_hz =
        std::lerp(spec.tone_start_hz, spec.tone_end_hz, along);
    const float cutoff =
        std::lerp(spec.noise_start_hz, spec.noise_end_hz, along);
    const float two_pi = 2.0F * std::numbers::pi_v<float>;
    state.phase = std::fmod(state.phase + two_pi * tone_hz / rate, two_pi);
    const float alpha = 1.0F - std::exp(-two_pi * cutoff / rate);
    state.filtered += alpha * (nextNoise(state.noise) - state.filtered);
    const float mix = spec.tone_level * std::sin(state.phase) +
                      spec.noise_level * state.filtered;
    return mix * envelope(spec, t);
  }

  /// Scale @p samples so the loudest is `PEAK`; silence stays silent.
  void normalise(std::vector<float>& samples) {
    float loudest = 0.0F;
    for (const float sample : samples) {
      loudest = std::max(loudest, std::abs(sample));
    }
    if (loudest <= 0.0F) {
      return;
    }
    for (float& sample : samples) {
      sample *= PEAK / loudest;
    }
  }

}  // namespace

AudioClip synthesize(const SynthSpec& spec, uint32_t sample_rate) {
  const float rate = static_cast<float>(std::max(sample_rate, 1U));
  const auto frames =
      static_cast<size_t>(std::max(1.0F, std::round(spec.seconds * rate)));
  AudioClip clip{.samples = std::vector<float>(frames),
                 .sample_rate = sample_rate,
                 .channels = 1};
  SynthState state{.noise = spec.seed == 0 ? 1U : spec.seed};
  for (size_t i = 0; i < frames; ++i) {
    clip.samples[i] =
        synthSample(spec, state, static_cast<float>(i) / rate, rate);
  }
  normalise(clip.samples);
  return clip;
}

}  // namespace eng::audio
