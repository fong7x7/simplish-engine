#pragma once

/// @file animation-clip.h
/// @brief A named motion — a walk, an idle — as the channels that make it.
/// @par Threading
/// Immutable value type once loaded; read from any thread.

#include <engine/animation/animation-channel.h>
#include <string>
#include <vector>

namespace eng::animation {

/// A named set of channels that play together.
///
/// A joint no channel drives keeps its rest pose while the clip plays, so a
/// clip that animates only the arms leaves the legs standing.
/// @thread_safety Immutable value type once loaded.
struct AnimationClip {
  /// The name the source file gave it, which is what a prop records to say
  /// which clip it plays.
  std::string name;
  /// Seconds from zero to the clip's last key: the length of one loop.
  float duration = 0.0f;
  /// What the clip drives, in any order.
  std::vector<AnimationChannel> channels;
};

}  // namespace eng::animation
