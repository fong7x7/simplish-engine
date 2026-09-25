#pragma once

/// @file logic-cue-reach.h
/// @brief Where a cue the logic raises is heard from.
/// @par Threading
/// A value type.

#include <cstdint>

namespace eng::game {

/// How a `LogicCue`'s sound reaches whoever is listening.
enum class LogicCueReach : uint8_t {
  /// From where it is: quieter further off, panned to its side.
  AT,
  /// Everywhere alike, at its own volume: an announcer, a horn for the
  /// whole arena.
  EVERYWHERE,
};

}  // namespace eng::game
