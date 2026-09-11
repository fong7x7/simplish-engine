#pragma once

/// @file editor-character-gait.h
/// @brief Whether a drawn character is standing or on the move.
/// @par Threading Immutable value type.

#include <cstdint>

namespace eng::editor {

/// What a character is doing, as far as picking its animation goes.
/// @thread_safety Immutable value type.
enum class EditorCharacterGait : uint8_t {
  /// Standing where the last tick left it.
  STILL,
  /// Moved on the last tick.
  MOVING,
};

}  // namespace eng::editor
