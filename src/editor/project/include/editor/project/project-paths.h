#pragma once

/// @file project-paths.h
/// @brief Canonical on-disk layout of a project directory.
/// @par Threading Thread-safe (pure functions over paths).

#include <filesystem>
#include <string_view>

namespace eng::editor {

/// Directory inside a project root holding editor-owned project files.
inline constexpr std::string_view PROJECT_DIR_NAME = ".simplish";
/// Project manifest file name inside `PROJECT_DIR_NAME`.
inline constexpr std::string_view PROJECT_FILE_NAME = "project.json";
/// Directory inside a project root holding authored content.
inline constexpr std::string_view PROJECT_DATA_DIR_NAME = "data";
/// Directory inside a project root holding importable source assets.
inline constexpr std::string_view PROJECT_ASSETS_DIR_NAME = "assets";
/// Directory inside a project root holding authored content files.
inline constexpr std::string_view PROJECT_CONTENT_DIR_NAME = "content";
/// Directory inside `PROJECT_CONTENT_DIR_NAME` holding level files.
inline constexpr std::string_view PROJECT_LEVELS_DIR_NAME = "levels";
/// Directory inside `PROJECT_DIR_NAME` holding generated asset thumbnails.
inline constexpr std::string_view PROJECT_THUMBNAILS_DIR_NAME = "thumbnails";

/// Path to a project root's editor directory (`<root>/.simplish`).
[[nodiscard]] inline std::filesystem::path
projectDirPath(const std::filesystem::path& root) {
  return root / PROJECT_DIR_NAME;
}

/// Path to a project root's manifest (`<root>/.simplish/project.json`).
[[nodiscard]] inline std::filesystem::path
projectFilePath(const std::filesystem::path& root) {
  return projectDirPath(root) / PROJECT_FILE_NAME;
}

/// Path to a project root's content directory (`<root>/data`).
[[nodiscard]] inline std::filesystem::path
projectDataPath(const std::filesystem::path& root) {
  return root / PROJECT_DATA_DIR_NAME;
}

/// Path to a project root's asset directory (`<root>/assets`), which the
/// editor's asset panel lists.
[[nodiscard]] inline std::filesystem::path
projectAssetsPath(const std::filesystem::path& root) {
  return root / PROJECT_ASSETS_DIR_NAME;
}

/// Path to a project root's authored content (`<root>/content`).
///
/// Separate from `assets/`, and the split is the point: assets are source
/// material the pipeline imports, content is what the editor authors over
/// them (docs/editor/project-format.md §2).
[[nodiscard]] inline std::filesystem::path
projectContentPath(const std::filesystem::path& root) {
  return root / PROJECT_CONTENT_DIR_NAME;
}

/// Path to a project root's level files (`<root>/content/levels`).
[[nodiscard]] inline std::filesystem::path
projectLevelsPath(const std::filesystem::path& root) {
  return projectContentPath(root) / PROJECT_LEVELS_DIR_NAME;
}

/// Path to a project root's generated thumbnails
/// (`<root>/.simplish/thumbnails`).
///
/// Under the editor's own directory rather than beside the assets: these
/// are derived files, cheap to rebuild, and nothing a person authored. A
/// project can be committed without them.
[[nodiscard]] inline std::filesystem::path
projectThumbnailsPath(const std::filesystem::path& root) {
  return projectDirPath(root) / PROJECT_THUMBNAILS_DIR_NAME;
}

}  // namespace eng::editor
