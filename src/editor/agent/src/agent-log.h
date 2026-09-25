#pragma once

/// @file agent-log.h
/// @brief The log tool: what the editor warned of, and nowhere showed.
/// @par Threading Main-thread-only.

#include <editor/agent/agent-result.h>
#include <editor/shell/editor-shell-state.h>
#include <nlohmann/json.hpp>

namespace eng::editor {

/// `get_log`: the editor's recent log lines — numbered, oldest first — at
/// or above a level, from a sequence number on, optionally one subsystem's.
AgentResult runAgentGetLog(EditorShellState& state,
                           const nlohmann::json& params);

}  // namespace eng::editor
