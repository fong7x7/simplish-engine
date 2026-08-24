#pragma once

/// @file recent-projects-list.h
/// @brief Ordered list of recently-opened projects.
/// @par Threading Main-thread-only.

#include <cstddef>
#include <editor/project/recent-project-entry.h>
#include <vector>

namespace eng::editor {

/// Maximum entries retained; older entries are dropped on insert.
inline constexpr size_t RECENT_PROJECTS_MAX = 10;

/// Recently-opened projects, most-recent first.
/// @thread_safety Main-thread-only.
struct RecentProjectsList {
  /// Entries ordered most-recent first, capped at `RECENT_PROJECTS_MAX`.
  std::vector<RecentProjectEntry> entries;
};

}  // namespace eng::editor
