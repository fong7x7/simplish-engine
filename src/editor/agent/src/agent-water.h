#pragma once

/// @file agent-water.h
/// @brief The water tools: how finely water is drawn, and what it is doing.
/// @par Threading Main-thread-only.

#include <editor/agent/agent-result.h>
#include <editor/shell/editor-property-field.h>
#include <editor/shell/editor-shell-state.h>
#include <nlohmann/json.hpp>

namespace eng::editor {

/// `get_water`: the user's water fidelity and the file it is kept in, and
/// what the level's water did last frame — whether a moving surface was
/// drawn, how finely it is simulated, how much of it there is, how much it
/// is moving, and how often it has been pushed.
AgentResult runAgentGetWater(EditorShellState& state,
                             const nlohmann::json& params);

/// `set_water_fidelity`: draw water flat, low or high from the next frame,
/// as the View menu's water rows do. Refused, changing nothing, on a word
/// that names no fidelity.
AgentResult runAgentSetWaterFidelity(EditorShellState& state,
                                     const nlohmann::json& params);

/// `set_water_depth`: make the water in a rectangle of cells, or in the
/// selected body of water, one depth, as one undoable edit.
AgentResult runAgentSetWaterDepth(EditorShellState& state,
                                  const nlohmann::json& params);

/// `paint_water`: lay water over a rectangle of cells — at a depth, and
/// where it was dry in a colour and opacity — or dry them, as the Water and
/// Dry cards do, as one undoable edit.
AgentResult runAgentPaintWater(EditorShellState& state,
                               const nlohmann::json& params);

/// Select the body of water at the call's `x` and `y`, as a click does.
AgentResult agentSelectWaterAt(EditorShellState& state,
                               const nlohmann::json& params);

/// Dry the selected body of water, as the Delete key does, as one undoable
/// edit, and clear the selection.
AgentResult agentDrySelectedWater(EditorShellState& state);

/// Set @p field — a colour channel or the opacity — of the selected body
/// of water to @p value, as its slider does, as one undoable edit.
AgentResult agentSetWaterField(EditorShellState& state,
                               EditorPropertyField field, float value);

}  // namespace eng::editor
