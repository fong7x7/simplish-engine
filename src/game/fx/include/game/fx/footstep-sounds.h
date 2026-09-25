#pragma once

/// @file footstep-sounds.h
/// @brief What a step sounds like: the clip each foot plays on each surface.
/// @par Threading
/// Pure functions, and loading into a bank on the main thread.
/// Presentation, run after the tick that moved the walker.
///
/// The footsteps' counterpart of `combat-sounds.h`. Every surface has a
/// built-in step, synthesised, under `step.default.<surface>`; a project
/// can record any step set on any surface, `step.<set>.<surface>`, and
/// whatever it has not recorded falls back to the nearest it has.

#include <array>
#include <cstddef>
#include <cstdint>
#include <engine/audio/audio-clip-bank.h>
#include <engine/audio/audio-synth.h>
#include <engine/audio/sound-play.h>
#include <engine/math/vec3.h>
#include <game/content/footstep-surface.h>
#include <game/content/step-set-stride.h>  // IWYU pragma: export
#include <game/content/step-set.h>
#include <game/fx/footstep-clip.h>
#include <game/fx/footstep-cue.h>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace eng::game {

/// The clip every step set plays on every surface: `[set][surface]`.
using FootstepSoundClips =
    std::array<std::array<FootstepClip, FOOTSTEP_SURFACE_COUNT>,
               STEP_SET_COUNT>;

/// The most steps heard from one tick. A horde walking is thousands of
/// feet; the nearest few are what the ear picks out of them.
inline constexpr size_t FOOTSTEPS_HEARD = 6;


/// What the bank calls the clip @p steps plays on @p surface:
/// `step.boots.sand`. Also the slot a project records it in.
[[nodiscard]] std::string footstepSoundName(StepSet steps,
                                            FootstepSurface surface);

/// The feet and surface a footstep slot is for — `step.boots.sand` is
/// boots on sand — or nothing when @p slot is not one.
[[nodiscard]] std::optional<std::pair<StepSet, FootstepSurface>>
footstepSlotNamed(std::string_view slot);

/// The recipe @p surface's built-in step is synthesised from.
[[nodiscard]] const audio::SynthSpec& footstepSynth(FootstepSurface surface);

/// Synthesise every surface's built-in step into @p bank at @p sample_rate,
/// under `step.default.<surface>`, replacing any already there.
void loadFootstepSounds(audio::AudioClipBank& bank, uint32_t sample_rate);

/// The clip every step set plays on every surface: the first of
/// `step.<set>.<surface>`, `step.<set>.ground`, `step.default.<surface>` and
/// `step.default.ground` there is. The first two are the set's own, and
/// count only when named in @p recorded — the slots a project has loaded a
/// file into — since a bank never forgets a name and a recording taken away
/// must stop playing; the last two are always in @p bank once
/// `loadFootstepSounds` has run, and stand in for the set.
[[nodiscard]] FootstepSoundClips
resolveFootstepClips(const audio::AudioClipBank& bank,
                     std::span<const std::string> recorded);

/// How @p cue is played with @p clips: from where the foot landed, at its
/// step set's loudness, quietly and below everything else when voices run
/// short, and a shade off pitch so a walk does not sound like a loop. A
/// stood-in clip is also shifted to the step set's own pitch.
[[nodiscard]] audio::SoundPlay footstepSound(const FootstepCue& cue,
                                             const FootstepSoundClips& clips);

/// Append to @p heard the nearest `FOOTSTEPS_HEARD` of @p cues to
/// @p listener, in the order they happened.
void hearFootsteps(std::span<const FootstepCue> cues, Vec3 listener,
                   std::vector<FootstepCue>& heard);

}  // namespace eng::game
