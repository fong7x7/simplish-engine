#pragma once

/// @file audio-bus.h
/// @brief The groups a mix is turned up and down by.
/// @par Threading
/// A value type.

#include <cstdint>

namespace eng::audio {

/// Which group a sound plays in. Each has its own volume, over the master
/// one, so a player can turn the music down without losing the gunfire, and
/// music is what a loud moment ducks. A closed set: a mixer holds a gain
/// for each.
enum class AudioBus : uint8_t {
  /// The world: shots, hits, blasts, footsteps.
  EFFECTS,
  /// The score.
  MUSIC,
  /// Menus and the HUD: clicks and confirmations, never placed in the world.
  INTERFACE,
};

/// How many buses there are.
inline constexpr uint8_t AUDIO_BUS_COUNT = 3;

}  // namespace eng::audio
