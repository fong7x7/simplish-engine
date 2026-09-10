#pragma once

/// @file agent-commands.h
/// @brief The tools that change the editor, one function each.
/// @par Threading Main-thread-only (edits shell state).

#include <editor/agent/agent-result.h>
#include <editor/shell/editor-shell-state.h>
#include <nlohmann/json.hpp>

namespace eng::editor {

/// One asset and its state, named by index or by name.
[[nodiscard]] AgentResult runAgentGetAsset(EditorShellState& state,
                                           const nlohmann::json& params);

/// Place an asset on a tile and select it, as a browser drag would.
[[nodiscard]] AgentResult runAgentPlaceAsset(EditorShellState& state,
                                             const nlohmann::json& params);

/// Add a light of the named kind and select it.
[[nodiscard]] AgentResult runAgentAddLight(EditorShellState& state,
                                           const nlohmann::json& params);

/// Add a player start at a position, for a named player or the lowest one
/// with no start yet, and select it.
[[nodiscard]] AgentResult runAgentAddPlayerStart(EditorShellState& state,
                                                 const nlohmann::json& params);

/// Write one property of one entry to an absolute value.
[[nodiscard]] AgentResult runAgentSetProperty(EditorShellState& state,
                                              const nlohmann::json& params);

/// Move one entry by a delta in tiles.
[[nodiscard]] AgentResult runAgentTranslate(EditorShellState& state,
                                            const nlohmann::json& params);

/// Take one entry back out of the level.
[[nodiscard]] AgentResult runAgentDelete(EditorShellState& state,
                                         const nlohmann::json& params);

/// Select an entry, or clear the selection.
[[nodiscard]] AgentResult runAgentSelect(EditorShellState& state,
                                         const nlohmann::json& params);

/// Choose the active toolbar tool.
[[nodiscard]] AgentResult runAgentSetTool(EditorShellState& state,
                                          const nlohmann::json& params);

/// Revert the newest edit, carrying the selection with it.
[[nodiscard]] AgentResult runAgentUndo(EditorShellState& state);

/// Reapply the newest reverted edit, carrying the selection with it.
[[nodiscard]] AgentResult runAgentRedo(EditorShellState& state);

/// Ask the editor to run a menu command, once it is one that would do
/// something.
[[nodiscard]] AgentResult runAgentRunCommand(const EditorShellState& state,
                                             const nlohmann::json& params);

/// Ask the editor to open a project directory.
[[nodiscard]] AgentResult runAgentOpenProject(const nlohmann::json& params);

/// Ask the editor to rescan the open project's assets.
[[nodiscard]] AgentResult runAgentRescanAssets(const EditorShellState& state);

/// Ask the editor to add a level to the open project and edit it.
[[nodiscard]] AgentResult runAgentCreateLevel(const EditorShellState& state,
                                              const nlohmann::json& params);

/// Ask the editor to edit another of the project's levels.
[[nodiscard]] AgentResult runAgentOpenLevel(const EditorShellState& state,
                                            const nlohmann::json& params);

/// Queue starting a playtest, when there is a level to play and none is
/// being played.
[[nodiscard]] AgentResult runAgentStartPlaytest(const EditorShellState& state);

/// Queue stopping the running playtest.
[[nodiscard]] AgentResult runAgentStopPlaytest(const EditorShellState& state);

/// Queue player 1's input for the next ticks of the running playtest.
[[nodiscard]] AgentResult runAgentSendInput(EditorShellState& state,
                                            const nlohmann::json& params);

}  // namespace eng::editor
