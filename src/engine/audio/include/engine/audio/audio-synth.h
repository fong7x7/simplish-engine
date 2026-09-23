#pragma once

/// @file audio-synth.h
/// @brief Sounds made from numbers, for a game with no recordings yet.
/// @par Threading
/// Pure functions.
///
/// A clip made from a decaying tone and a burst of filtered noise is enough
/// to tell a shot from a blast by ear. The game's built-in combat sounds are
/// made this way until recorded ones replace them, so a playtest is heard
/// from the first run, and the same spec always makes the same samples.

#include <cstdint>
#include <engine/audio/audio-clip.h>

namespace eng::audio {

/// The recipe for a synthesised one-shot: a tone that sweeps from one pitch
/// to another, and white noise through a low-pass that sweeps with it, both
/// under one envelope — a linear attack, then an exponential decay.
struct SynthSpec {
  /// How long the clip is, in seconds.
  float seconds = 0.2F;
  /// How long it takes to reach full volume, in seconds.
  float attack_seconds = 0.002F;
  /// The decay's time constant: after this many seconds past the attack,
  /// it has fallen to about a third.
  float decay_seconds = 0.05F;
  /// The tone's pitch at the start, in hertz.
  float tone_start_hz = 440.0F;
  /// The tone's pitch at the end, in hertz; it sweeps between the two.
  float tone_end_hz = 440.0F;
  /// How loud the tone is, 0 to 1.
  float tone_level = 0.0F;
  /// How loud the noise is, 0 to 1.
  float noise_level = 1.0F;
  /// The noise filter's cutoff at the start, in hertz.
  float noise_start_hz = 8000.0F;
  /// The noise filter's cutoff at the end, in hertz.
  float noise_end_hz = 8000.0F;
  /// Seeds the noise, so the same spec makes the same clip.
  uint32_t seed = 1;
};

/// A mono clip at @p sample_rate, made from @p spec.
[[nodiscard]] AudioClip synthesize(const SynthSpec& spec, uint32_t sample_rate);

}  // namespace eng::audio
