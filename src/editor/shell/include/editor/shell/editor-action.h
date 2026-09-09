#pragma once

/// @file editor-action.h
/// @brief One recorded, reversible editor operation.
/// @par Threading Main-thread-only.

#include <cstddef>
#include <editor/shell/editor-action-kind.h>
#include <editor/shell/editor-placement.h>

namespace eng::editor {

/// A mutating operation, holding everything needed to undo and redo it.
///
/// Actions are values, not command objects with virtual `apply` and
/// `revert`. There is one document and a small set of operations on it, so
/// a tagged record keeps the history copyable and comparable, keeps every
/// operation's inverse readable in one file, and costs no allocation per
/// edit — where a class hierarchy would add one heap node per action and
/// spread the inverses across as many files as there are tools.
/// @thread_safety Main-thread-only.
struct EditorAction {
  /// Which operation this record describes.
  EditorActionKind kind = EditorActionKind::PLACE_ASSET;
  /// Where in the placement list the operation added or removed an entry.
  size_t index = 0;
  /// The placement added or removed, kept so redo restores it exactly.
  EditorPlacement placement{};
};

}  // namespace eng::editor
