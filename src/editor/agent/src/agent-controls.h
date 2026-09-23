#pragma once

/// @file agent-controls.h
/// @brief The tools that read and change the user's control scheme.
/// @par Threading Main-thread-only (edits shell state).

#include <editor/agent/agent-result.h>
#include <editor/shell/editor-shell-state.h>
#include <nlohmann/json.hpp>

namespace eng::editor {

/// Every action's controls, the deadzones and the file, as the bindings
/// file lays them out.
[[nodiscard]] AgentResult runAgentGetControls(EditorShellState& state,
                                              const nlohmann::json& params);

/// Reset, rebind one action and set deadzones, as the call asks, then
/// answer as `get_controls` does. The editor saves on its next frame.
[[nodiscard]] AgentResult runAgentSetControls(EditorShellState& state,
                                              const nlohmann::json& params);

}  // namespace eng::editor
