#pragma once

/// @file agent-animation-events.h
/// @brief The tools that read and write the sounds animations make.
/// @par Threading Main-thread-only (reads and edits shell state).

#include <editor/agent/agent-result.h>
#include <editor/shell/editor-shell-state.h>
#include <nlohmann/json.hpp>

namespace eng::editor {

/// Every loaded rig's clips and the events each plays, where they came
/// from, and the animation events table as written.
[[nodiscard]] AgentResult
runAgentListAnimationEvents(EditorShellState& state,
                            const nlohmann::json& params);

/// Give one clip of a rigged model, or one sprite sheet, its events — or
/// take its row away, leaving a clip to its detected foot contacts.
[[nodiscard]] AgentResult
runAgentSetAnimationEvents(EditorShellState& state,
                           const nlohmann::json& params);

}  // namespace eng::editor
