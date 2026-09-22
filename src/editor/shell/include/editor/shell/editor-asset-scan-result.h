#pragma once

/// @file editor-asset-scan-result.h
/// @brief What one pass over a project's assets directory found.
/// @par Threading Main-thread-only.

#include <editor/shell/editor-asset.h>
#include <filesystem>
#include <vector>

namespace eng::editor {

/// The assets, the sprite sheets and the directories a scan turned up, in
/// one result.
///
/// Directories are carried alongside the assets rather than derived from
/// them, because a directory holding nothing yet is still somewhere to drop
/// a file, and the browser should show it.
/// @thread_safety Main-thread-only.
struct EditorAssetScan {
  /// Every mesh file found, sorted by path relative to the assets root.
  std::vector<EditorAsset> assets;
  /// Every directory found, as paths relative to the assets root, sorted.
  std::vector<std::filesystem::path> folders;
  /// Every sprite sheet found, as paths relative to the assets root,
  /// sorted.
  ///
  /// Paths rather than `EditorAsset`s: a sheet is not placeable and has no
  /// mesh, no thumbnail and no index for a placement to name. What points
  /// at one is a billboard's `sheet` field, which holds this path.
  std::vector<std::filesystem::path> sheets;
};

}  // namespace eng::editor
