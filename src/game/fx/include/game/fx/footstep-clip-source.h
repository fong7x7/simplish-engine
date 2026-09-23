#pragma once

/// @file footstep-clip-source.h
/// @brief Whether a footstep's clip was recorded for its feet.
/// @par Threading
/// A value type.

#include <cstdint>

namespace eng::game {

/// Where the clip a step set plays on a surface came from.
enum class FootstepClipSource : uint8_t {
  /// Recorded for this step set: played as it is.
  OWN,
  /// Another step set's — the default's — standing in: played at this
  /// step set's pitch, so boots on it still sound heavier than bare feet.
  BORROWED,
};

}  // namespace eng::game
