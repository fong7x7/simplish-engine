#pragma once

/// @file editor-asset-tree.h
/// @brief The folder hierarchy the asset browser navigates.
/// @par Threading Main-thread-only.

#include <cstddef>
#include <editor/shell/editor-asset-folder.h>
#include <editor/shell/editor-asset-scan-result.h>
#include <vector>

namespace eng::editor {

/// Index of the assets root. Every tree has one, so this is always valid,
/// and it is the folder the browser opens on.
inline constexpr size_t EDITOR_ASSET_FOLDER_ROOT = 0;

/// Every folder the browser lists, flattened into one vector.
///
/// The assets themselves stay in the flat list the scan produced, and the
/// folders hold indices into it. Placements index that same list, so
/// grouping assets by folder costs their numbering nothing. A folder may
/// also hold entries past the end of it — see `editor-general-section.h`,
/// which is what those numbers mean.
/// @thread_safety Main-thread-only.
struct EditorAssetTree {
  /// Folders, with the root at `EDITOR_ASSET_FOLDER_ROOT`. Never empty.
  std::vector<EditorAssetFolder> folders{EditorAssetFolder{}};
  /// The parentless folders, in the order the pane lists them.
  ///
  /// Held rather than derived: which section comes first is a decision
  /// about the pane, and a tree that answers it cannot have it re-derived
  /// two different ways by two callers. A scan produces the assets root
  /// alone; `editor-general-section.h` adds the built-in one.
  std::vector<size_t> sections{EDITOR_ASSET_FOLDER_ROOT};
};

/// Group a scan into its folder hierarchy.
///
/// Every directory the scan found becomes a node, whether or not it holds
/// any assets, and so does every directory on the path to an asset. An
/// empty scan yields a tree holding only the root.
[[nodiscard]] EditorAssetTree buildEditorAssetTree(const EditorAssetScan& scan);

}  // namespace eng::editor
