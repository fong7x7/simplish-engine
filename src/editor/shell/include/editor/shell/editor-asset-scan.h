#pragma once

/// @file editor-asset-scan.h
/// @brief Discovery of placeable assets under a project.
/// @par Threading Main-thread-only (reads the filesystem).

#include <editor/shell/editor-asset.h>
#include <filesystem>
#include <vector>

namespace eng::editor {

/// File extension the asset panel lists. OBJ is the one format the mesh
/// loader reads; anything else in the directory is ignored rather than
/// listed as broken.
inline constexpr std::string_view ASSET_MESH_EXTENSION = ".obj";

/// List the placeable assets directly under @p assets_dir, sorted by name
/// so the panel's order does not depend on directory iteration order.
///
/// A missing or unreadable directory yields an empty list, which is the
/// same thing the panel shows for a project with no assets yet.
[[nodiscard]] std::vector<EditorAsset>
scanEditorAssets(const std::filesystem::path& assets_dir);

}  // namespace eng::editor
