#pragma once

/// @file editor-asset-scan.h
/// @brief Discovery of placeable assets under a project.
/// @par Threading Main-thread-only (reads the filesystem).

#include <editor/shell/editor-asset-scan-result.h>
#include <filesystem>
#include <string_view>

namespace eng::editor {

/// File extensions the asset panel lists: static OBJ models, and rigged
/// glTF ones in either of glTF's two forms. Anything else in the directory
/// is ignored rather than listed as broken. Matched without regard to case.
inline constexpr std::string_view ASSET_MESH_EXTENSIONS[] = {".obj", ".gltf",
                                                             ".glb"};

/// File extensions the scan lists as sprite sheets: the two raster formats
/// that carry an alpha channel and that every sprite tool exports. Matched
/// without regard to case.
///
/// Alpha is the point. A billboard's empty texels are cut out at
/// `MESH_ALPHA_CUTOFF`, so a format with no alpha channel draws the whole
/// frame rectangle, background and all.
inline constexpr std::string_view ASSET_SHEET_EXTENSIONS[] = {".png", ".tga"};

/// File extensions the scan lists as sounds: the two formats `engine/audio`
/// decodes — WAV and Ogg Vorbis. Matched without regard to case.
inline constexpr std::string_view ASSET_SOUND_EXTENSIONS[] = {".wav", ".ogg"};

/// Whether @p path is a sound file, by its extension.
[[nodiscard]] bool isSoundFile(const std::filesystem::path& path);

/// Whether @p path is a rigged model — a glTF file, read by the skinned
/// loader and drawn posed — rather than a static OBJ. Decided by extension,
/// as the scan decides what to list.
[[nodiscard]] bool isRiggedModelFile(const std::filesystem::path& path);

/// List the placeable assets, the sprite sheets, the sounds and the
/// directories under
/// @p assets_dir, walking sub-directories, sorted by relative path so the
/// browser's order does not depend on directory iteration order.
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
