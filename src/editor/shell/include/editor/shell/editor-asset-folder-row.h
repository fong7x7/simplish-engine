#pragma once

/// @file editor-asset-folder-row.h
/// @brief One line in the asset browser's folder pane.
/// @par Threading Main-thread-only.

#include <cstddef>
#include <cstdint>
#include <editor/shell/editor-asset-folder.h>

namespace eng::editor {

/// A folder as the pane draws it: which folder, how far in, and whether it
/// is open.
///
/// The pane lists a tree as a flat run of rows, because that is what hit
/// testing and scrolling want. Everything needed to draw and click one row
/// is here, so neither pass has to walk the tree again.
/// @thread_safety Immutable value type.
struct EditorAssetFolderRow {
  /// The folder, as an index into `EditorAssetTree::folders`.
  size_t folder = 0;
  /// Nesting depth, which sets the indent. The root is 0.
  uint32_t depth = 0;
  /// Whether this folder's children are listed below it.
  bool expanded = false;
  /// Whether it has children at all, and so whether it can be opened.
  bool has_children = false;
};

}  // namespace eng::editor
