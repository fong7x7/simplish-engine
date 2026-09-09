#pragma once

/// @file editor-property-edit.h
/// @brief Whether a property change is still in flight or finished.
/// @par Threading Thread-safe (immutable value type).

#include <cstdint>

namespace eng::editor {

/// How far along a property edit is.
///
/// Dragging a value emits a run of `PREVIEW` changes and one `COMMIT` on
/// release; a step button emits a single `COMMIT`. The editor applies both
/// the same way and records history only on the commit, so a drag across
/// two hundred pixels is one entry in the undo list rather than two hundred.
/// @thread_safety Immutable value type.
enum class EditorPropertyEdit : uint8_t {
  /// The value changed, and the gesture producing it is still running.
  PREVIEW,
  /// The gesture finished; this is the value to record.
  COMMIT,
};

}  // namespace eng::editor
