#pragma once

/// @file recent-project-entry.h
/// @brief One row in the recent-projects list.
/// @par Threading Main-thread-only.

#include <string>

namespace eng::editor {

/// A previously-opened project, as remembered for the launcher list.
/// @thread_safety Value type.
struct RecentProjectEntry {
  /// Absolute path to the project root, as a UTF-8 string.
  std::string path;
  /// Display name captured when the project was last opened.
  std::string name;
  /// ISO 8601 timestamp of the last time the project was opened.
  std::string last_opened_at;
};

}  // namespace eng::editor
