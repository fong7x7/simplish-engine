#pragma once

/// @file agent-build.h
/// @brief The build tool: the project's game logic, and its deployed game.
/// @par Threading Main-thread-only.

#include <editor/agent/agent-result.h>
#include <editor/shell/editor-shell-state.h>
#include <nlohmann/json.hpp>
#include <string>

namespace eng::editor {

/// Everything `get_build` reports, as JSON text.
[[nodiscard]] std::string agentBuildJson(const EditorShellState& state);

/// `get_build`: whether the project has game logic of its own, whether a
/// build of it is loaded and current, how the last build went — with the
/// compiler's complaints — and where the last deploy put the game. The
/// Build menu's commands are run with `run_command`; this is how an agent
/// sees what they did.
AgentResult runAgentGetBuild(EditorShellState& state,
                             const nlohmann::json& params);

}  // namespace eng::editor
