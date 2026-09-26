#pragma once

/// @file agent-interface.h
/// @brief The interface tool: how large the editor's own GUI is drawn.
/// @par Threading Main-thread-only.

#include <editor/agent/agent-result.h>
#include <editor/shell/editor-shell-state.h>
#include <nlohmann/json.hpp>

namespace eng::editor {

/// `set_interface_size`: draw the editor's interface at a scale from 0.5
/// to 3, as View › Interface does, from the next frame; the user's
/// setting, saved with their graphics settings. Refused, changing nothing,
/// on a scale out of range.
AgentResult runAgentSetInterfaceSize(EditorShellState& state,
                                     const nlohmann::json& params);

}  // namespace eng::editor
