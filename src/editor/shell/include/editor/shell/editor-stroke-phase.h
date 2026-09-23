#pragma once

/// @file editor-stroke-phase.h
/// @brief Where in a paint stroke the viewport is.
/// @par Threading Thread-safe (immutable value type).

#include <cstdint>

namespace eng::editor {

/// One moment of a paint stroke, as the viewport reports it: a stroke is
/// one press, any number of moves while it is held, and one release, and
/// is recorded as one undoable edit however many cells it crossed.
/// @thread_safety Immutable value type.
enum class EditorStrokePhase : uint8_t {
  /// The button went down: paint here, and start remembering.
  BEGIN,
  /// The cursor moved while it was held: paint here too.
  MOVE,
  /// The button came up: the stroke is over, and is one edit.
  END,
};

}  // namespace eng::editor
