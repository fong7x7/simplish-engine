#pragma once

/// @file editor-asset-scan.h
/// @brief Discovery of placeable assets under a project.
/// @par Threading Main-thread-only (reads the filesystem).

#include <editor/shell/editor-asset-scan-result.h>
#include <filesystem>
#include <string_view>

namespace eng::editor {

/// File extension the asset panel lists. OBJ is the one format the mesh
/// loader reads; anything else in the directory is ignored rather than
/// listed as broken.
inline constexpr std::string_view ASSET_MESH_EXTENSION = ".obj";

/// List the placeable assets and directories under @p assets_dir, walking
/// sub-directories, sorted by relative path so the browser's order does not
/// depend on directory iteration order.
///
/// Directories whose name starts with a dot are skipped whole: they hold
/// editor and version-control metadata, not content.
///
/// A missing or unreadable directory yields an empty result, which is the
/// same thing the browser shows for a project with no assets yet. An
/// unreadable sub-directory costs only its own subtree.
[[nodiscard]] EditorAssetScan
scanEditorAssets(const std::filesystem::path& assets_dir);

}  // namespace eng::editor
