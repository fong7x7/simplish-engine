#include <algorithm>
#include <bit>
#include <cmath>
#include <game/fx/combat-sounds.h>

namespace eng::game {

namespace {

  /// How a kind of cue sounds.
  struct CombatSound {
    /// What the bank calls its clip.
    std::string_view name;
    /// Its clip's recipe.
    audio::SynthSpec synth;
    /// Its volume.
    float gain = 1.0F;
    /// How much it matters when voices run out.
    uint8_t priority = audio::SOUND_PRIORITY_DEFAULT;
  };

  /// Every kind's sound, by `CombatCueKind`.
  constexpr std::array<CombatSound, COMBAT_CUE_KIND_COUNT> SOUNDS{{
      // A shot: a cracking burst of bright noise closing down fast, over a
      // falling knock.
      {.name = "combat.shot_fired",
       .synth = {.seconds = 0.22F,
                 .attack_seconds = 0.001F,
                 .decay_seconds = 0.045F,
                 .tone_start_hz = 180.0F,
                 .tone_end_hz = 60.0F,
                 .tone_level = 0.5F,
                 .noise_start_hz = 9000.0F,
                 .noise_end_hz = 1500.0F,
                 .seed = 11},
       .gain = 0.55F,
       .priority = 100},
      // A hit: a low, dull thud.
      {.name = "combat.shot_hit_body",
       .synth = {.seconds = 0.16F,
                 .decay_seconds = 0.04F,
                 .tone_start_hz = 120.0F,
                 .tone_end_hz = 70.0F,
                 .tone_level = 0.8F,
                 .noise_level = 0.5F,
                 .noise_start_hz = 1200.0F,
                 .noise_end_hz = 400.0F,
                 .seed = 23},
       .gain = 0.6F,
       .priority = 120},
      // A wall: a short high tick with a ring that falls away.
      {.name = "combat.shot_hit_wall",
       .synth = {.seconds = 0.18F,
                 .decay_seconds = 0.03F,
                 .tone_start_hz = 2400.0F,
                 .tone_end_hz = 1400.0F,
                 .tone_level = 0.35F,
                 .noise_level = 0.9F,
                 .noise_start_hz = 7000.0F,
                 .noise_end_hz = 3000.0F,
                 .seed = 37},
       .gain = 0.4F,
       .priority = 80},
      // A blast: a long rumble whose noise darkens as it rolls away.
      {.name = "combat.blast",
       .synth = {.seconds = 1.4F,
                 .attack_seconds = 0.004F,
                 .decay_seconds = 0.35F,
                 .tone_start_hz = 70.0F,
                 .tone_end_hz = 35.0F,
                 .tone_level = 0.7F,
                 .noise_start_hz = 3000.0F,
                 .noise_end_hz = 200.0F,
                 .seed = 53},
       .gain = 1.0F,
       .priority = 220},
  }};

  /// A player's own shot is louder, and matters more, than an actor's.
  constexpr float FRIENDLY_SHOT_GAIN = 0.75F;
  /// How much a player's own shot matters.
  constexpr uint8_t FRIENDLY_SHOT_PRIORITY = 160;
  /// How far off true pitch a cue may be, either way.
  constexpr float PITCH_SPREAD = 0.08F;

  /// @p kind's sound.
  const CombatSound& soundOf(CombatCueKind kind) {
    return SOUNDS[static_cast<size_t>(kind)];
  }

  /// A pitch near one that @p cue always gets, from where it happened:
  /// presentation, so a hash of its place rather than a random stream.
  float pitchOf(const CombatCue& cue) {
    uint32_t hash = std::bit_cast<uint32_t>(cue.at.x) * 0x9E3779B1U;
    hash ^= std::bit_cast<uint32_t>(cue.at.y) * 0x85EBCA77U;
    hash ^= hash >> 15U;
    const float unit = static_cast<float>(hash & 0xFFFFU) / 65535.0F;
    return 1.0F + (unit * 2.0F - 1.0F) * PITCH_SPREAD;
  }

  /// Whether @p cue is a player's own shot.
  bool friendlyShot(const CombatCue& cue) {
    return cue.kind == CombatCueKind::SHOT_FIRED &&
           cue.side == Faction::FRIENDLY;
  }

  /// How far @p cue is from @p listener across the ground, squared.
  float distance2(const CombatCue& cue, Vec3 listener) {
    const float dx = cue.at.x - listener.x;
    const float dy = cue.at.y - listener.y;
    return dx * dx + dy * dy;
  }

  /// The nearest cues of one kind found so far.
  struct NearestCues {
    /// Their indices into the tick's cues, in no order.
    std::array<size_t, COMBAT_SOUNDS_PER_KIND> index{};
    /// How many of `index` are filled.
    size_t count = 0;
  };

  /// Keep cue @p i of @p cues in @p nearest if it is among the nearest to
  /// @p listener so far: in a free place, or in place of the furthest if
  /// it is nearer still. Offered in order, so a tie keeps the earlier.
  void offerCue(NearestCues& nearest, size_t i, std::span<const CombatCue> cues,
                Vec3 listener) {
    if (nearest.count < COMBAT_SOUNDS_PER_KIND) {
      nearest.index[nearest.count++] = i;
      return;
    }
    size_t furthest = 0;
    for (size_t k = 1; k < COMBAT_SOUNDS_PER_KIND; ++k) {
      if (distance2(cues[nearest.index[k]], listener) >
          distance2(cues[nearest.index[furthest]], listener)) {
        furthest = k;
      }
    }
    if (distance2(cues[i], listener) <
        distance2(cues[nearest.index[furthest]], listener)) {
      nearest.index[furthest] = i;
    }
  }

}  // namespace

std::string_view combatSoundName(CombatCueKind kind) {
  return soundOf(kind).name;
}

const audio::SynthSpec& combatSoundSynth(CombatCueKind kind) {
  return soundOf(kind).synth;
}

CombatSoundClips loadCombatSounds(audio::AudioClipBank& bank,
                                  uint32_t sample_rate) {
  CombatSoundClips clips{};
  for (size_t i = 0; i < SOUNDS.size(); ++i) {
    clips[i] = bank.add(SOUNDS[i].name,
                        audio::synthesize(SOUNDS[i].synth, sample_rate));
  }
  return clips;
}

audio::SoundPlay combatCueSound(const CombatCue& cue,
                                const CombatSoundClips& clips) {
  const CombatSound& sound = soundOf(cue.kind);
  const bool own = friendlyShot(cue);
  return {.clip = clips[static_cast<size_t>(cue.kind)],
          .gain = own ? FRIENDLY_SHOT_GAIN : sound.gain,
          .pitch = pitchOf(cue),
          .placement = audio::SoundPlacement::IN_WORLD,
          .at = cue.at,
          .ducking = cue.kind == CombatCueKind::BLAST
                         ? audio::SoundDucking::DUCKS_MUSIC
                         : audio::SoundDucking::NONE,
          .priority = own ? FRIENDLY_SHOT_PRIORITY : sound.priority};
}

void hearCombatCues(std::span<const CombatCue> cues, Vec3 listener,
                    std::vector<CombatCue>& heard) {
  std::array<NearestCues, COMBAT_CUE_KIND_COUNT> nearest{};
  for (size_t i = 0; i < cues.size(); ++i) {
    offerCue(nearest[static_cast<size_t>(cues[i].kind)], i, cues, listener);
  }
  std::array<size_t, COMBAT_SOUNDS_PER_KIND * COMBAT_CUE_KIND_COUNT> kept{};
  size_t count = 0;
  for (const NearestCues& kind : nearest) {
    for (size_t k = 0; k < kind.count; ++k) {
      kept[count++] = kind.index[k];
    }
  }
  std::sort(kept.begin(), kept.begin() + static_cast<ptrdiff_t>(count));
  for (size_t k = 0; k < count; ++k) {
    heard.push_back(cues[kept[k]]);
  }
}

}  // namespace eng::game
