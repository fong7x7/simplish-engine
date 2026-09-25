#pragma once

/// @file player-change-kind.h
/// @brief What became of a downed player.
/// @par Threading
/// A value type.

#include <cstdint>

namespace eng::game {

/// How a downed player's tick ended, when it ended in anything.
enum class PlayerChangeKind : uint8_t {
  /// A teammate stood by them long enough: they are up again.
  REVIVED,
  /// The window ran out, or nobody was left to come: they are out.
  OUT,
};

}  // namespace eng::game
