#pragma once

/// @file sound-placement.h
/// @brief Whether a sound comes from somewhere in the world.
/// @par Threading
/// A value type.

#include <cstdint>

namespace eng::audio {

/// Where a sound is heard from.
enum class SoundPlacement : uint8_t {
  /// Nowhere: straight into both ears at its own volume — music, menus.
  FLAT,
  /// A point in the world: quieter the further it is from the listener,
  /// and panned toward the side of the screen it is on.
  IN_WORLD,
};

}  // namespace eng::audio
