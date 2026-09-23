#pragma once

/// @file combat-sounds.h
/// @brief What a fight sounds like: the sound each combat cue plays.
/// @par Threading
/// Pure functions, and loading into a bank on the main thread.
/// Presentation, run after the tick that cued it.
///
/// The audio counterpart of `combat-fx.h`: a crack for a shot fired, a thud
/// for one that lands, a tick for one a wall stops, a boom that ducks the
/// music for a blast. The clips are synthesised (`audio-synth.h`) until
/// recorded ones exist; when sound becomes content, this is the table the
/// content replaces.

#include <array>
#include <cstddef>
#include <engine/audio/audio-clip-bank.h>
#include <engine/audio/audio-clip-id.h>
#include <engine/audio/audio-synth.h>
#include <engine/audio/sound-play.h>
#include <engine/math/vec3.h>
#include <game/combat/combat-cue-kind.h>
#include <game/combat/combat-cue.h>
#include <span>
#include <string_view>
#include <vector>

namespace eng::game {

/// The clip each kind of cue plays, by `CombatCueKind`, in a bank.
using CombatSoundClips = std::array<audio::AudioClipId, COMBAT_CUE_KIND_COUNT>;

/// The most cues of one kind heard from one tick. A horde firing at once is
/// a wall of cues; past the nearest few, another copy of the same crack is
/// noise that takes voices from the sounds that matter.
inline constexpr size_t COMBAT_SOUNDS_PER_KIND = 4;

/// What the bank calls the clip a cue of @p kind plays: `combat.blast`.
[[nodiscard]] std::string_view combatSoundName(CombatCueKind kind);

/// The recipe a cue of @p kind's built-in clip is synthesised from.
[[nodiscard]] const audio::SynthSpec& combatSoundSynth(CombatCueKind kind);

/// Synthesise every built-in combat sound at @p sample_rate into @p bank,
/// replacing any already there under the same names, and give their ids.
CombatSoundClips loadCombatSounds(audio::AudioClipBank& bank,
                                  uint32_t sample_rate);

/// How @p cue is played with @p clips: from where it happened, louder and
/// more important for a player's own shot than an actor's, a blast above
/// all and ducking the music, and each a shade off in pitch so a burst of
/// the same shot does not sound like a loop.
[[nodiscard]] audio::SoundPlay combatCueSound(const CombatCue& cue,
                                              const CombatSoundClips& clips);

/// Append to @p heard the cues in @p cues worth hearing from @p listener:
/// of each kind, the nearest `COMBAT_SOUNDS_PER_KIND`, in the order they
/// happened.
void hearCombatCues(std::span<const CombatCue> cues, Vec3 listener,
                    std::vector<CombatCue>& heard);

}  // namespace eng::game
