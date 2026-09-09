#pragma once

/// @file project-ops.h
/// @brief Open, create, and remember projects on disk.
/// @par Threading Main-thread-only (touches the filesystem).

#include <editor/project/project-context.h>
#include <editor/project/project-open-result.h>
#include <editor/project/recent-projects-list.h>
#include <filesystem>
#include <string>
#include <string_view>

namespace eng::editor {

/// True when @p root holds a readable `.simplish/project.json`.
[[nodiscard]] bool isProjectDirectory(const std::filesystem::path& root);

/// Open the project rooted at @p root.
///
/// Validates that the path exists, is a directory, and holds a parseable
/// manifest. Does not mutate the project — `last_opened_at` is stamped by
/// `touchProjectOpened`, so a failed session never rewrites the manifest.
[[nodiscard]] ProjectOpenResult openProject(const std::filesystem::path& root);

/// Create a project directory at @p root and open it.
///
/// Creates `<root>/.simplish/project.json` and `<root>/data/`. Fails with
/// ALREADY_EXISTS when a manifest is already present, so this never
/// overwrites an existing project.
[[nodiscard]] ProjectOpenResult createProject(const std::filesystem::path& root,
                                              std::string_view name,
                                              std::string_view timestamp);

/// Stamp `last_opened_at` on an open project and write the manifest back.
/// Returns false when the write fails; the in-memory context is still updated.
bool touchProjectOpened(ProjectContext& context, std::string_view timestamp);

/// Write the open project's manifest back to disk, for a setting the editor
/// changed. False when no project is open or the write fails.
[[nodiscard]] bool saveProjectMetadata(const ProjectContext& context);

/// Load the recent-projects list from @p path. Returns an empty list when the
/// file is absent or unreadable — a missing list is a normal first-run state,
/// not an error.
[[nodiscard]] RecentProjectsList
loadRecentProjects(const std::filesystem::path& path);

/// Write @p list to @p path, creating parent directories as needed.
bool saveRecentProjects(const RecentProjectsList& list,
                        const std::filesystem::path& path);

/// Move @p context to the front of @p list, de-duplicating by path and
/// trimming to `RECENT_PROJECTS_MAX`.
void promoteRecentProject(RecentProjectsList& list,
                          const ProjectContext& context,
                          std::string_view timestamp);

/// Remove the entry whose path matches @p path. Returns true if one was
/// removed.
bool removeRecentProject(RecentProjectsList& list, std::string_view path);

/// Current UTC time as an ISO 8601 string (`YYYY-MM-DDTHH:MM:SSZ`).
///
/// Editor-only: no simulation code may call this, per the determinism
/// contract in docs/decisions/ADR-002-fixed-timestep-determinism.md.
[[nodiscard]] std::string isoTimestampNow();

}  // namespace eng::editor
