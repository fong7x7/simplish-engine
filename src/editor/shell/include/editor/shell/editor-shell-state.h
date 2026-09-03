#pragma once

/// @file editor-shell-state.h
/// @brief Editor state that outlives any one frame.
/// @par Threading Main-thread-only.

#include <editor/project/project-context.h>
#include <editor/project/recent-projects-list.h>
#include <editor/shell/editor-asset.h>
#include <editor/shell/editor-placement.h>
#include <editor/shell/editor-tool.h>
#include <filesystem>
#include <vector>

namespace eng::editor {

/// State the editor shell carries across frames.
/// @thread_safety Main-thread-only.
struct EditorShellState {
  /// The open project, or a default-constructed context when none is open.
  ProjectContext project;
  /// Recently-opened projects, loaded at startup.
  RecentProjectsList recent;
  /// Where the recent-projects list is persisted.
  std::filesystem::path recent_path;
  /// Currently selected authoring tool.
  EditorTool active_tool = EditorTool::SELECT;
  /// Assets found under the open project, in panel order.
  std::vector<EditorAsset> assets;
  /// Assets placed in the world. In memory only — see `editor-placement.h`.
  std::vector<EditorPlacement> placements;
};

}  // namespace eng::editor
