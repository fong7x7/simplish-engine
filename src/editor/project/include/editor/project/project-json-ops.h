#pragma once

/// @file project-json-ops.h
/// @brief JSON serialisation for project types.
/// @par Threading Thread-safe (pure functions over strings).

#include <editor/project/project-metadata.h>
#include <editor/project/recent-projects-list.h>
#include <optional>
#include <string>
#include <string_view>

namespace eng::editor {

/// Parse project metadata. Returns nullopt when @p json is not valid JSON.
/// Missing fields fall back to their defaults rather than failing.
[[nodiscard]] std::optional<ProjectMetadata>
parseProjectMetadata(std::string_view json);

/// Serialise project metadata as pretty-printed JSON.
[[nodiscard]] std::string serializeProjectMetadata(const ProjectMetadata& meta);

/// Parse a recent-projects list. Returns nullopt when @p json is not valid
/// JSON. Entries missing a `path` are skipped rather than failing the parse.
[[nodiscard]] std::optional<RecentProjectsList>
parseRecentProjects(std::string_view json);

/// Serialise a recent-projects list as pretty-printed JSON.
[[nodiscard]] std::string
serializeRecentProjects(const RecentProjectsList& list);

}  // namespace eng::editor
