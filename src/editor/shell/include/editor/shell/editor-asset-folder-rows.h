#pragma once

/// @file editor-asset-folder-rows.h
/// @brief Flattening the folder tree into the rows the pane lists.
/// @par Threading Thread-safe (pure function over value types).

#include <cstddef>
#include <editor/shell/editor-asset-folder-row.h>
#include <editor/shell/editor-asset-tree.h>
#include <unordered_set>
#include <vector>

namespace eng::editor {

/// Flatten @p tree into the rows to draw, descending only into folders
/// listed in @p expanded.
///
/// The root is always the first row, so the pane always offers somewhere to
/// go back to. A folder in @p expanded that this tree does not have is
/// ignored, so a set left over from a larger tree cannot walk off the end
/// of this one. It will still name the wrong folders, though — indices
/// only mean anything against the tree they came from — so a caller whose
/// tree has been rebuilt should start the set again rather than lean on
/// this.
[[nodiscard]] std::vector<EditorAssetFolderRow>
flattenAssetFolderRows(const EditorAssetTree& tree,
                       const std::unordered_set<size_t>& expanded);

}  // namespace eng::editor
