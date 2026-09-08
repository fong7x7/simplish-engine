#pragma once

/// @file editor-asset-tree.h
/// @brief The folder hierarchy the asset browser navigates.
/// @par Threading Main-thread-only.

#include <cstddef>
#include <editor/shell/editor-asset-folder.h>
#include <editor/shell/editor-asset-scan-result.h>
#include <vector>

namespace eng::editor {

/// Index of the root folder. Every tree has one, so this is always valid.
inline constexpr size_t EDITOR_ASSET_FOLDER_ROOT = 0;

/// Every folder under a project's assets root, flattened into one vector.
///
/// The assets themselves stay in the flat list the scan produced, and the
/// folders hold indices into it. Placements index that same list, so
/// grouping assets by folder costs their numbering nothing.
/// @thread_safety Main-thread-only.
struct EditorAssetTree {
  /// Folders, with the root at `EDITOR_ASSET_FOLDER_ROOT`. Never empty.
  std::vector<EditorAssetFolder> folders{EditorAssetFolder{}};
};

/// Group a scan into its folder hierarchy.
///
/// Every directory the scan found becomes a node, whether or not it holds
/// any assets, and so does every directory on the path to an asset. An
/// empty scan yields a tree holding only the root.
[[nodiscard]] EditorAssetTree buildEditorAssetTree(const EditorAssetScan& scan);

}  // namespace eng::editor
