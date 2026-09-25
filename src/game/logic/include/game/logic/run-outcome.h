#pragma once

/// @file run-outcome.h
/// @brief How a run stands: still going, won or lost.
/// @par Threading
/// A value type.

#include <cstdint>

namespace eng::game {

/// Whether a run is over, and how. Only game logic decides a win; a run is
/// lost either by logic saying so or by every player being out of it.
enum class RunOutcome : uint8_t {
  /// Still being played.
  PLAYING,
  /// Over, and won.
  WON,
  /// Over, and lost.
  LOST,
};

}  // namespace eng::game
