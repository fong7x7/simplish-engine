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

/// Write one property of one entry to an absolute value.
[[nodiscard]] AgentResult runAgentSetProperty(EditorShellState& state,
                                              const nlohmann::json& params);

/// Move one entry by a delta in tiles.
[[nodiscard]] AgentResult runAgentTranslate(EditorShellState& state,
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

}  // namespace eng::editor
