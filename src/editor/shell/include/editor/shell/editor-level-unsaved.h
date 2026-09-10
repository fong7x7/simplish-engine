#pragma once

/// @file editor-level-unsaved.h
/// @brief What a level switch does about edits that are not in a file.
/// @par Threading Thread-safe (immutable value type).

#include <cstdint>

namespace eng::editor {

/// What switching level does when the open one holds unwritten edits.
///
/// Named rather than a flag because the two answers are not more and less
/// of one thing: one of them loses work. The menu passes `REFUSE` and says
/// so in the status line; an agent has to ask for `DISCARD` in as many
/// words.
/// @thread_safety Immutable value type.
enum class EditorLevelUnsaved : uint8_t {
  /// Refuse the switch and leave the open level as it is.
  REFUSE,
  /// Throw the unwritten edits away and switch.
  DISCARD,
};

}  // namespace eng::editor
