#pragma once

/// @file agent-emitters.h
/// @brief The tools that place particle emitters and change what they
/// throw: adding one, starting it from a preset, and writing or moving it.
/// @par Threading Main-thread-only (edits shell state).

#include <cstddef>
#include <editor/agent/agent-result.h>
#include <editor/shell/editor-property-field.h>
#include <editor/shell/editor-shell-state.h>
#include <nlohmann/json.hpp>
#include <string>

namespace eng::editor {

/// Add a particle emitter at a position, started from a named preset or the
/// default one, and select it.
[[nodiscard]] AgentResult runAgentAddEmitter(EditorShellState& state,
                                             const nlohmann::json& params);

/// Start an emitter from a named preset: its burst and flash become the
/// preset's, as the Effect row does, as one undoable edit.
[[nodiscard]] AgentResult runAgentSetEffect(EditorShellState& state,
                                            const nlohmann::json& params);

/// Write one field of the emitter at @p index, as one undoable edit.
[[nodiscard]] AgentResult setAgentEmitterField(EditorShellState& state,
                                               size_t index,
                                               EditorPropertyField field,
                                               float value);

/// Move the emitter at @p index by the call's dx, dy and dz, as one
/// undoable edit.
[[nodiscard]] AgentResult translateAgentEmitter(EditorShellState& state,
                                                size_t index,
                                                const nlohmann::json& params);

/// The emitter at @p index as this API reports it, index included.
[[nodiscard]] std::string agentEmitterPayload(const EditorShellState& state,
                                              size_t index);

/// Every preset an emitter can be started from, by id and name.
[[nodiscard]] nlohmann::json agentEffectPresetsJson();

}  // namespace eng::editor
