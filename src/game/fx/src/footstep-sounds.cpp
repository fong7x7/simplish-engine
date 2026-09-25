#include <algorithm>
#include <bit>
#include <game/content/footstep-names.h>
#include <game/fx/footstep-sounds.h>
#include <numeric>
#include <utility>

namespace eng::game {

namespace {

  /// How a step set sounds, whatever it walks on.
  struct StepVoice {
    /// Its loudness.
    float gain = 0.3F;
    /// The pitch a stood-in clip is shifted to.
    float pitch = 1.0F;
  };

  /// Every step set's voice, by `StepSet`.
  constexpr std::array<StepVoice, STEP_SET_COUNT> VOICES{{
      {.gain = 0.30F, .pitch = 1.0F},
      {.gain = 0.36F, .pitch = 0.92F},
      {.gain = 0.20F, .pitch = 1.18F},
      {.gain = 0.26F, .pitch = 1.4F},
      {.gain = 0.50F, .pitch = 0.68F},
  }};

  /// Every surface's built-in step, by `FootstepSurface`.
  constexpr std::array<audio::SynthSpec, FOOTSTEP_SURFACE_COUNT> SYNTHS{{
      // Ground: a dull scuff over a soft knock.
      {.seconds = 0.14F,
       .decay_seconds = 0.035F,
       .tone_start_hz = 140.0F,
       .tone_end_hz = 90.0F,
       .tone_level = 0.35F,
       .noise_start_hz = 2400.0F,
       .noise_end_hz = 900.0F,
       .seed = 101},
      // Grass: a swish and nothing under it.
      {.seconds = 0.16F,
       .attack_seconds = 0.004F,
       .decay_seconds = 0.045F,
       .noise_level = 0.9F,
       .noise_start_hz = 3500.0F,
       .noise_end_hz = 2200.0F,
       .seed = 103},
      // Dirt: a low, packed thud.
      {.seconds = 0.13F,
       .decay_seconds = 0.03F,
       .tone_start_hz = 110.0F,
       .tone_end_hz = 80.0F,
       .tone_level = 0.5F,
       .noise_start_hz = 1800.0F,
       .noise_end_hz = 700.0F,
       .seed = 107},
      // Sand: a bright crunch that lingers.
      {.seconds = 0.18F,
       .attack_seconds = 0.006F,
       .decay_seconds = 0.05F,
       .noise_start_hz = 5000.0F,
       .noise_end_hz = 2600.0F,
       .seed = 109},
      // Water: a splash, and a falling plop in it.
      {.seconds = 0.26F,
       .attack_seconds = 0.004F,
       .decay_seconds = 0.08F,
       .tone_start_hz = 420.0F,
       .tone_end_hz = 260.0F,
       .tone_level = 0.25F,
       .noise_start_hz = 2600.0F,
       .noise_end_hz = 1200.0F,
       .seed = 113},
      // Stone: a short hard click.
      {.seconds = 0.12F,
       .attack_seconds = 0.001F,
       .decay_seconds = 0.025F,
       .tone_start_hz = 900.0F,
       .tone_end_hz = 700.0F,
       .tone_level = 0.35F,
       .noise_start_hz = 6500.0F,
       .noise_end_hz = 3000.0F,
       .seed = 127},
      // Wood: a hollow knock.
      {.seconds = 0.16F,
       .attack_seconds = 0.001F,
       .decay_seconds = 0.04F,
       .tone_start_hz = 230.0F,
       .tone_end_hz = 170.0F,
       .tone_level = 0.8F,
       .noise_level = 0.5F,
       .noise_start_hz = 3000.0F,
       .noise_end_hz = 1200.0F,
       .seed = 131},
      // Metal: a clank with a ring.
      {.seconds = 0.3F,
       .attack_seconds = 0.001F,
       .decay_seconds = 0.09F,
       .tone_start_hz = 1600.0F,
       .tone_end_hz = 1500.0F,
       .tone_level = 0.6F,
       .noise_level = 0.6F,
       .noise_start_hz = 7000.0F,
       .noise_end_hz = 4000.0F,
       .seed = 137},
      // Cloth: barely anything.
      {.seconds = 0.12F,
       .attack_seconds = 0.006F,
       .decay_seconds = 0.04F,
       .noise_level = 0.6F,
       .noise_start_hz = 900.0F,
       .noise_end_hz = 500.0F,
       .seed = 139},
  }};

  /// How much a step matters when voices run out: below everything else.
  constexpr uint8_t FOOTSTEP_PRIORITY = 50;
  /// How far off pitch a step may be, either way.
  constexpr float PITCH_SPREAD = 0.06F;

  /// @p steps's voice.
  const StepVoice& voiceOf(StepSet steps) {
    return VOICES[static_cast<size_t>(steps)];
  }

  /// Where one step set looks for its clips: the bank, and the slots a
  /// project has recorded.
  struct ClipSources {
    /// Every clip loaded.
    const audio::AudioClipBank& bank;
    /// The slots holding a project's own file.
    std::span<const std::string> recorded;
  };

  /// The clip the slot @p name plays, if it counts: one of the default
  /// set's, which always do, or one @p from recorded.
  std::optional<audio::AudioClipId>
  slotClip(const ClipSources& from, const std::string& name, StepSet steps) {
    const bool counts =
        steps == StepSet::DEFAULT ||
        std::ranges::find(from.recorded, name) != from.recorded.end();
    return counts ? from.bank.find(name) : std::nullopt;
  }

  /// The clip @p steps plays on @p surface, falling back as
  /// `resolveFootstepClips` says.
  FootstepClip resolveOne(const ClipSources& from, StepSet steps,
                          FootstepSurface surface) {
    const std::array<std::pair<StepSet, FootstepSurface>, 4> chain{{
        {steps, surface},
        {steps, FootstepSurface::GROUND},
        {StepSet::DEFAULT, surface},
        {StepSet::DEFAULT, FootstepSurface::GROUND},
    }};
    for (const auto& [set, on] : chain) {
      if (const auto id = slotClip(from, footstepSoundName(set, on), set)) {
        return {*id, set == steps ? FootstepClipSource::OWN
                                  : FootstepClipSource::BORROWED};
      }
    }
    return {};
  }

  /// A pitch near one that a step at @p at always gets: presentation, so a
  /// hash of its place rather than a random stream.
  float spreadAt(Vec3 at) {
    uint32_t hash = std::bit_cast<uint32_t>(at.x) * 0x9E3779B1U;
    hash ^= std::bit_cast<uint32_t>(at.y) * 0x85EBCA77U;
    hash ^= hash >> 15U;
    const float unit = static_cast<float>(hash & 0xFFFFU) / 65535.0F;
    return 1.0F + (unit * 2.0F - 1.0F) * PITCH_SPREAD;
  }

  /// How far @p cue is from @p listener across the ground, squared.
  float distance2(const FootstepCue& cue, Vec3 listener) {
    const float dx = cue.at.x - listener.x;
    const float dy = cue.at.y - listener.y;
    return dx * dx + dy * dy;
  }

}  // namespace

std::string footstepSoundName(StepSet steps, FootstepSurface surface) {
  return "step." + std::string(stepSetWord(steps)) + "." +
         std::string(footstepSurfaceWord(surface));
}

std::optional<std::pair<StepSet, FootstepSurface>>
footstepSlotNamed(std::string_view slot) {
  for (const StepSet steps : ALL_STEP_SETS) {
    for (const FootstepSurface surface : ALL_FOOTSTEP_SURFACES) {
      if (footstepSoundName(steps, surface) == slot) {
        return std::pair{steps, surface};
      }
    }
  }
  return std::nullopt;
}

const audio::SynthSpec& footstepSynth(FootstepSurface surface) {
  return SYNTHS[static_cast<size_t>(surface)];
}

void loadFootstepSounds(audio::AudioClipBank& bank, uint32_t sample_rate) {
  for (const FootstepSurface surface : ALL_FOOTSTEP_SURFACES) {
    bank.add(footstepSoundName(StepSet::DEFAULT, surface),
             audio::synthesize(footstepSynth(surface), sample_rate));
  }
}

FootstepSoundClips resolveFootstepClips(const audio::AudioClipBank& bank,
                                        std::span<const std::string> recorded) {
  const ClipSources from{bank, recorded};
  FootstepSoundClips clips{};
  for (const StepSet steps : ALL_STEP_SETS) {
    for (const FootstepSurface surface : ALL_FOOTSTEP_SURFACES) {
      clips[static_cast<size_t>(steps)][static_cast<size_t>(surface)] =
          resolveOne(from, steps, surface);
    }
  }
  return clips;
}

audio::SoundPlay footstepSound(const FootstepCue& cue,
                               const FootstepSoundClips& clips) {
  const FootstepClip& clip =
      clips[static_cast<size_t>(cue.steps)][static_cast<size_t>(cue.surface)];
  const StepVoice& voice = voiceOf(cue.steps);
  const float pitch =
      clip.source == FootstepClipSource::BORROWED ? voice.pitch : 1.0F;
  return {.clip = clip.clip,
          .gain = voice.gain,
          .pitch = pitch * spreadAt(cue.at),
          .placement = audio::SoundPlacement::IN_WORLD,
          .at = cue.at,
          .priority = FOOTSTEP_PRIORITY};
}

void hearFootsteps(std::span<const FootstepCue> cues, Vec3 listener,
                   std::vector<FootstepCue>& heard) {
  std::vector<size_t> order(cues.size());
  std::iota(order.begin(), order.end(), size_t{0});
  const size_t kept = std::min(cues.size(), FOOTSTEPS_HEARD);
  // Nearest first, the earlier of two alike; then back into the order they
  // happened in.
  std::ranges::stable_sort(order, [&](size_t a, size_t b) {
    return distance2(cues[a], listener) < distance2(cues[b], listener);
  });
  order.resize(kept);
  std::ranges::sort(order);
  for (const size_t i : order) {
    heard.push_back(cues[i]);
  }
}

}  // namespace eng::game
