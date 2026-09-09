#pragma once

/// @file project-metadata.h
/// @brief Parsed contents of `.simplish/project.json`.
/// @par Threading Main-thread-only.

#include <editor/project/project-projection.h>
#include <string>

namespace eng::editor {

/// Parsed contents of `.simplish/project.json`.
/// @thread_safety Value type; main-thread-only in practice.
struct ProjectMetadata {
  /// Human-readable project display name.
  std::string name;
  /// Semantic version of the engine that created the project.
  std::string engine_version;
  /// ISO 8601 timestamp of project creation.
  std::string created_at;
  /// ISO 8601 timestamp of the last editor session.
  std::string last_opened_at;
  /// Workspace to open on launch — see docs/editor/REQUIREMENTS.md §3.
  std::string default_workspace{"Level"};
  /// The projection this project's world is drawn and authored with.
  ProjectProjection projection = ProjectProjection::DIMETRIC;
};

}  // namespace eng::editor
