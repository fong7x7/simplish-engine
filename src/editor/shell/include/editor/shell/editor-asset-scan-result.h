#pragma once

/// @file editor-asset-scan-result.h
/// @brief What one pass over a project's assets directory found.
/// @par Threading Main-thread-only.

#include <editor/shell/editor-asset.h>
#include <filesystem>
#include <vector>

namespace eng::editor {

/// The assets and the directories a scan turned up, in one result.
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
};

}  // namespace eng::editor
