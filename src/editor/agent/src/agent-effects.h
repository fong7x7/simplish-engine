#pragma once

/// @file agent-effects.h
/// @brief The tools that show effects on request and report what the
/// viewport's effects are doing.
/// @par Threading Main-thread-only.

#include <editor/agent/agent-result.h>
#include <editor/shell/editor-shell-state.h>
#include <nlohmann/json.hpp>
#include <string>

namespace eng::editor {

/// Play an effect once — a preset, a whole combat effect, or an emitter's
/// own burst — into whatever effects the viewport is drawing. Carried out
/// by the running editor, since the effects live there.
[[nodiscard]] AgentResult runAgentPlayEffect(EditorShellState& state,
                                             const nlohmann::json& params);

/// What the viewport's effects are doing: particles and flashes live, whose
/// they are, bursts each emitter has thrown, and effects played on request.
[[nodiscard]] std::string agentEffectsStateJson(const EditorShellState& state);

}  // namespace eng::editor
