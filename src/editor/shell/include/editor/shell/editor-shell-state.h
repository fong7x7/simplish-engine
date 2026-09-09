#pragma once

/// @file editor-shell-state.h
/// @brief Editor state that outlives any one frame.
/// @par Threading Main-thread-only.

#include <editor/project/project-context.h>
#include <editor/project/recent-projects-list.h>
#include <editor/shell/editor-action-history.h>
#include <editor/shell/editor-asset-tree.h>
#include <editor/shell/editor-asset.h>
#include <editor/shell/editor-document.h>
#include <editor/shell/editor-selection.h>
#include <editor/shell/editor-tool.h>
#include <editor/shell/editor-view-state.h>
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
  /// Assets found under the open project, in scan order. Placements
  /// index into this list, so it is the numbering that must stay put.
  std::vector<EditorAsset> assets;
  /// What the browser lists: the built-in general section, and the folders
  /// those assets sit in. Both hold entry numbers, of which the assets are
  /// the first `assets.size()`.
  EditorAssetTree asset_tree;
  /// What has been placed and what lights it. In memory only — see
  /// `editor-document.h`.
  EditorDocument document;
  /// The one entry of that document the properties panel edits, or nothing.
  EditorSelection selection;
  /// Every edit made to `document` this session, and the undo cursor into
  /// them. Cleared with the document, since it describes it by index.
  EditorActionHistory history;
  /// Where the viewport camera sits and what it is over, refreshed from
  /// the widget once a tick. Read-only — see `editor-view-state.h`.
  EditorViewState view;
};

}  // namespace eng::editor
