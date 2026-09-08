#pragma once

/// @file editor-asset-folder.h
/// @brief One directory node in the asset browser's folder tree.
/// @par Threading Main-thread-only.

#include <cstddef>
#include <filesystem>
#include <string>
#include <vector>

namespace eng::editor {

/// Index standing for "no folder": what the root reports as its parent.
inline constexpr size_t EDITOR_ASSET_FOLDER_NONE = static_cast<size_t>(-1);

/// A directory under the project's assets root, and what it holds.
///
/// Children are held as indices rather than pointers or nested nodes, so
/// the whole tree is one flat vector. That makes it copyable, and it lets a
/// widget remember which folder it is showing as a plain number.
/// @thread_safety Main-thread-only.
struct EditorAssetFolder {
  /// Directory name, for display. Empty for the root.
  std::string name;
  /// Path relative to the assets root. Empty for the root.
  std::filesystem::path relative_path;
  /// Enclosing folder, or `EDITOR_ASSET_FOLDER_NONE` for the root.
  size_t parent = EDITOR_ASSET_FOLDER_NONE;
  /// Child folders, as indices into `EditorAssetTree::folders`, name-sorted.
  std::vector<size_t> child_folders{};
  /// Assets held directly here, as indices into the scanned asset list,
  /// name-sorted.
  std::vector<size_t> assets{};
};

}  // namespace eng::editor
