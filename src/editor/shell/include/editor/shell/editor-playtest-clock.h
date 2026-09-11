#pragma once

/// @file editor-playtest-clock.h
/// @brief Whether a running playtest's clock is advancing.
/// @par Threading Thread-safe (immutable value type).

#include <cstdint>

namespace eng::editor {

/// Whether a playtest runs on real time or waits to be stepped.
///
/// Paused, the game stays exactly where it is — nothing is simulated, and
/// the frames the clock would have paid for are dropped rather than owed —
/// until it is resumed or stepped a tick at a time, which is how a decision
/// an actor made is found on the tick it made it (Editor REQUIREMENTS §7).
/// @thread_safety Immutable value type.
enum class EditorPlaytestClock : uint8_t {
  /// Ticks run as real time pays for them.
  RUNNING,
  /// Ticks run only when stepped.
  PAUSED,
};

}  // namespace eng::editor
