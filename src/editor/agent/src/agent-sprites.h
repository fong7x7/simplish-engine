#pragma once

/// @file agent-sprites.h
/// @brief The tools that place sprite billboards and change what they show:
/// adding one, pointing it at a sheet, and writing or moving it.
/// @par Threading Main-thread-only (edits shell state).

#include <cstddef>
#include <editor/agent/agent-result.h>
#include <editor/shell/editor-property-field.h>
#include <editor/shell/editor-shell-state.h>
#include <nlohmann/json.hpp>
#include <string>

namespace eng::editor {

/// Add a sprite billboard at a position, showing a named sheet or the
/// project's first one, and select it.
[[nodiscard]] AgentResult runAgentAddSprite(EditorShellState& state,
                                            const nlohmann::json& params);

/// Point a billboard at another sheet, as the Sheet row does, as one
/// undoable edit.
[[nodiscard]] AgentResult runAgentSetSheet(EditorShellState& state,
                                           const nlohmann::json& params);

/// Write one field of the billboard at @p index, as one undoable edit.
[[nodiscard]] AgentResult setAgentSpriteField(EditorShellState& state,
                                              size_t index,
                                              EditorPropertyField field,
                                              float value);

/// Move the billboard at @p index by the call's dx, dy and dz, as one
/// undoable edit.
[[nodiscard]] AgentResult translateAgentSprite(EditorShellState& state,
                                               size_t index,
                                               const nlohmann::json& params);

/// The billboard at @p index as this API reports it, index included.
[[nodiscard]] std::string agentSpritePayload(const EditorShellState& state,
                                             size_t index);

/// Every sprite sheet the open project holds, by the path a billboard
/// names it with.
[[nodiscard]] nlohmann::json
agentSpriteSheetsJson(const EditorShellState& state);

}  // namespace eng::editor
