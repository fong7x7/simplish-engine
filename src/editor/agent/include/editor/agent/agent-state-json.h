#pragma once

/// @file agent-state-json.h
/// @brief The editor's state, as the JSON an agent reads.
/// @par Threading Main-thread-only (reads shell state).

#include <cstddef>
#include <editor/shell/editor-shell-state.h>
#include <string>

namespace eng::editor {

/// The editor at a glance: project, active tool, camera, selection, what
/// the document holds, and whether undo and redo have anything to do.
[[nodiscard]] std::string agentStateJson(const EditorShellState& state);

/// Every scanned asset, with its index and its load and thumbnail state.
[[nodiscard]] std::string agentAssetsJson(const EditorShellState& state);

/// One asset by index. The caller has already checked the index is one the
/// list has.
[[nodiscard]] std::string agentAssetJson(const EditorShellState& state,
                                         size_t index);

/// The asset browser's folder tree, the built-in general section included.
[[nodiscard]] std::string agentFoldersJson(const EditorShellState& state);

/// Every placement in the level.
[[nodiscard]] std::string agentPlacementsJson(const EditorShellState& state);

/// Every light in the level.
[[nodiscard]] std::string agentLightsJson(const EditorShellState& state);

/// Every player start in the level, and how many players a session holds.
[[nodiscard]] std::string agentPlayerStartsJson(const EditorShellState& state);

/// Whether the level is being played, and what the playtest has done.
[[nodiscard]] std::string agentPlaytestJson(const EditorShellState& state);

/// `list_characters`: every character in the project's table, and what was
/// wrong with the file.
[[nodiscard]] std::string agentCharactersJson(const EditorShellState& state);

/// Every behavior a prop can name — the built-in ones and the project's —
/// with the behaviors table's path and what was wrong with it.
[[nodiscard]] std::string agentBehaviorsJson(const EditorShellState& state);

/// What the properties panel is editing, and the fields it lists for it.
[[nodiscard]] std::string agentSelectionJson(const EditorShellState& state);

/// The session's edits, oldest first, and how many are applied.
[[nodiscard]] std::string agentHistoryJson(const EditorShellState& state);

/// The level file behind the document, and whether the two agree.
[[nodiscard]] std::string agentLevelJson(const EditorShellState& state);

/// Every level the open project holds, and which one is being edited.
[[nodiscard]] std::string agentLevelsJson(const EditorShellState& state);

/// Every menu command, and whether it would do anything right now.
[[nodiscard]] std::string agentCommandsJson(const EditorShellState& state);

/// Every tool this editor offers and what each takes: the manifest the MCP
/// bridge turns into its own tool list.
[[nodiscard]] std::string agentManifestJson();

/// The manifest plus what the editor currently has open — what `describe`
/// answers, and what a caller with no other documentation starts from.
[[nodiscard]] std::string agentDescribeJson(const EditorShellState& state);

}  // namespace eng::editor
