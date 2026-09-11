#pragma once

/// @file agent-waypoints.h
/// @brief The tools that lay out patrol routes: adding a waypoint, and
/// writing or moving one.
/// @par Threading Main-thread-only (edits shell state).

#include <cstddef>
#include <editor/agent/agent-result.h>
#include <editor/shell/editor-property-field.h>
#include <editor/shell/editor-shell-state.h>
#include <nlohmann/json.hpp>
#include <string>

namespace eng::editor {

/// Add a waypoint at a position — to a named route or the selected
/// waypoint's, after its last waypoint unless a place is named — and
/// select it.
[[nodiscard]] AgentResult runAgentAddWaypoint(EditorShellState& state,
                                              const nlohmann::json& params);

/// Write one field of the waypoint at @p index, as one undoable edit.
[[nodiscard]] AgentResult setAgentWaypointField(EditorShellState& state,
                                                size_t index,
                                                EditorPropertyField field,
                                                float value);

/// Move the waypoint at @p index by the call's dx, dy and dz, as one
/// undoable edit.
[[nodiscard]] AgentResult translateAgentWaypoint(EditorShellState& state,
                                                 size_t index,
                                                 const nlohmann::json& params);

/// The waypoint at @p index as this API reports it, index included.
[[nodiscard]] std::string agentWaypointPayload(const EditorShellState& state,
                                               size_t index);

}  // namespace eng::editor
