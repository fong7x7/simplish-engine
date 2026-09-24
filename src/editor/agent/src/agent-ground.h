#pragma once

/// @file agent-ground.h
/// @brief The tools that read and paint the ground.
/// @par Threading Main-thread-only (reads and edits shell state).

#include <editor/agent/agent-result.h>
#include <editor/shell/editor-shell-state.h>
#include <engine/render-ground/ground-rect.h>
#include <nlohmann/json.hpp>
#include <optional>

namespace eng::editor {

/// The most cells one `get_ground` reports: a window larger than this is
/// refused with a message naming the window to ask for instead.
inline constexpr int64_t AGENT_GROUND_READ_MAX_CELLS = 65536;

/// The terrains, and the painted cells inside a window of the ground — the
/// painted part of it when the call names none.
[[nodiscard]] AgentResult runAgentGetGround(const EditorShellState& state,
                                            const nlohmann::json& params);

/// Paint a rectangle of cells with one terrain, as one undoable edit.
[[nodiscard]] AgentResult runAgentPaintGround(EditorShellState& state,
                                              const nlohmann::json& params);

/// The rectangle a call names by its `x` and `y`, and its `width` and
/// `height` from 1 to `EDITOR_GROUND_FILL_MAX`, each 1 when left out;
/// nothing when it names one wrongly.
[[nodiscard]] std::optional<GroundRect>
agentFillParam(const nlohmann::json& params);

}  // namespace eng::editor
