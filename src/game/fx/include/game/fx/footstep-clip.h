#pragma once

/// @file footstep-clip.h
/// @brief The clip one step set plays on one surface.
/// @par Threading
/// A value type.

#include <engine/audio/audio-clip-id.h>
#include <game/fx/footstep-clip-source.h>

namespace eng::game {

/// What one step set sounds like on one surface: a clip in the bank, and
/// whether it was recorded for these feet or is standing in.
struct FootstepClip {
  /// The clip.
  audio::AudioClipId clip{};
  /// Whether it is this step set's own.
  FootstepClipSource source = FootstepClipSource::BORROWED;
};

}  // namespace eng::game
