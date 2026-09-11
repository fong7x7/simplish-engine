#pragma once

/// @file editor-shell-state.h
/// @brief Editor state that outlives any one frame.
/// @par Threading Main-thread-only.

#include <editor/project/project-context.h>
#include <editor/project/recent-projects-list.h>
#include <editor/shell/editor-action-history.h>
#include <editor/shell/editor-asset-tree.h>
#include <editor/shell/editor-asset.h>
#include <editor/shell/editor-behavior-table.h>
#include <editor/shell/editor-character-table.h>
#include <editor/shell/editor-document.h>
#include <editor/shell/editor-enemy-table.h>
#include <editor/shell/editor-level-entry.h>
#include <editor/shell/editor-level-json.h>
#include <editor/shell/editor-playtest-state.h>
#include <editor/shell/editor-selection.h>
#include <editor/shell/editor-tool.h>
#include <editor/shell/editor-view-state.h>
#include <filesystem>
#include <string>
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
  /// Which of the project's levels is open, by id. The file it is written
  /// to is `<root>/content/levels/<level_id>.level.json`.
  std::string level_id{EDITOR_LEVEL_ID};
  /// Every level the project holds, by id, in id order. Refreshed from
  /// disk when a project is opened and when a level is created, so the
  /// menu and the agent API both read it rather than the filesystem.
  std::vector<EditorLevelEntry> levels;
  /// What has been placed in the open level and what lights it. Written to
  /// that level's file on save and read back when one is opened — see
  /// `editor-level-io.h`.
  EditorDocument document;
  /// The one entry of that document the properties panel edits, or nothing.
  EditorSelection selection;
  /// Every edit made to `document` this session, the undo cursor into them,
  /// and whether any of them are unwritten. Cleared with the document,
  /// since it describes it by index.
  EditorActionHistory history;
  /// Whether the open project's level file was read, or there was none to
  /// read. False only when a file is there and could not be parsed.
  ///
  /// This is what stops a save from writing an empty level over a file
  /// somebody has mistyped by hand: the editor cannot show what it could
  /// not read, and overwriting it would turn a typo into a lost level.
  bool level_readable = true;
  /// Where the viewport camera sits and what it is over, refreshed from
  /// the widget once a tick. Read-only — see `editor-view-state.h`.
  EditorViewState view;
  /// The project's characters, from `content/data/characters.data.json`.
  /// Read when the project is opened, when its assets are rescanned, and
  /// every time Play is pressed — so a hand edit to the file reaches the
  /// next playtest without reopening anything.
  EditorCharacterTable characters;
  /// The project's behaviors, from `content/data/behaviors.data.json` —
  /// read when the characters are, for the same reason. The built-in
  /// behaviors are not here; `editorAvailableBehaviors` adds them.
  EditorBehaviorTable behaviors;
  /// The project's enemy archetypes, from `content/data/enemies.data.json`
  /// — read with the others. Nothing spawns them yet; the director will.
  EditorEnemyTable enemies;
  /// Whether the level is being played, and what the playtest has done —
  /// refreshed from the running game after every frame of play.
  EditorPlaytestState playtest;
};

}  // namespace eng::editor
