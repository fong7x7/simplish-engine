#pragma once

/// @file clip-event-crossing.h
/// @brief Which of a clip's marked moments a frame's playback passed.
/// @par Threading
/// Pure function.

#include <cstddef>
#include <engine/animation/clip-window.h>
#include <span>
#include <vector>

namespace eng::animation {

/// Append to @p crossed the index of every time in @p times — seconds into
/// a clip that loops every @p duration seconds — that playback passed in
/// @p window, in the order the playhead reached them.
///
/// A time exactly at the window's end counts; one exactly at its start was
/// counted by the window before. A window as long as the clip or longer —
/// a paused frame, a hitch — passes each time once, not once per loop it
/// skipped, so a stall is not followed by a burst. A clip of no length
/// passes nothing.
///
/// What a marked moment means is the caller's: a foot landing, a sword
/// swinging. Presentation, like every clip time (ADR-003 amendment).
void crossedClipTimes(std::span<const float> times, float duration,
                      ClipWindow window, std::vector<size_t>& crossed);

}  // namespace eng::animation
