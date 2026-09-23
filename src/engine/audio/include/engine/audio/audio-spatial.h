#pragma once

/// @file audio-spatial.h
/// @brief How loud a sound in the world is in each ear.
/// @par Threading
/// Pure functions.

#include <engine/audio/audio-listener.h>
#include <engine/audio/stereo-gain.h>
#include <engine/math/vec3.h>

namespace eng::audio {

/// How far toward one ear a sound can be panned, 0 to 1. Short of one, so a
/// sound off to the side is still heard a little in the other ear and a
/// horde on the left does not go silent on the right.
inline constexpr float AUDIO_MAX_PAN = 0.7F;

/// How loud a sound at @p at is for @p listener, 0 to 1: full within
/// `full_tiles` across the ground, silent past `silent_tiles`, and falling
/// along a square law between them.
[[nodiscard]] float distanceGain(const AudioListener& listener, Vec3 at);

/// Where a sound at @p at sits between the ears, -1 left to 1 right: how
/// far it is along the listener's screen-right, as a share of `full_tiles`,
/// and never past `AUDIO_MAX_PAN`.
[[nodiscard]] float pan(const AudioListener& listener, Vec3 at);

/// @p pan, -1 to 1, as an equal-power gain for each ear, scaled so the
/// centre is one in both: a sound in front is as loud as a flat one.
[[nodiscard]] StereoGain panGain(float pan);

/// The gain in each ear of a sound at @p at: its distance's gain, panned.
[[nodiscard]] StereoGain spatialGain(const AudioListener& listener, Vec3 at);

}  // namespace eng::audio
