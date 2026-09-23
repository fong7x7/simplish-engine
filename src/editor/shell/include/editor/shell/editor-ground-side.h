#pragma once

/// @file editor-ground-side.h
/// @brief Which side of a ground edit to write.
/// @par Threading Thread-safe (immutable value type).

#include <cstdint>

namespace eng::editor {

/// Which terrain `applyEditorGroundChanges` writes into each cell: what the
/// edit left, which is doing or redoing it, or what it found, which is
/// undoing it.
/// @thread_safety Immutable value type.
enum class EditorGroundSide : uint8_t {
  /// Each change's `before`.
  BEFORE,
  /// Each change's `after`.
  AFTER,
};

}  // namespace eng::editor
