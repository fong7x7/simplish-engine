#include "agent-emitters.h"

#include "agent-call.h"
#include "agent-json-values.h"

#include <editor/shell/editor-action-ops.h>
#include <editor/shell/editor-emitter-ops.h>
#include <editor/shell/editor-entity-id.h>
#include <game/fx/combat-fx-preset.h>
#include <optional>
#include <string>

namespace eng::editor {

namespace {

  using nlohmann::json;

  /// What `add_emitter` says when it is called wrongly.
  constexpr std::string_view ADD_EMITTER_USAGE =
      "x and y are required; effect, when given, is a preset id that "
      "list_emitters lists under effects";

  /// What `set_effect` says when it is called wrongly.
  constexpr std::string_view SET_EFFECT_USAGE =
      "expected target \"emitter\" with an index, or \"selection\" when an "
      "emitter is selected, and effect: a preset id that list_emitters "
      "lists under effects";

  /// Record a changed emitter as one undoable edit, and select it.
  AgentResult recordEmitter(EditorShellState& state, size_t index,
                            const EditorEmitter& prior,
                            const EditorEmitter& next) {
    performEditorAction(state.history, state.document,
                        {.kind = EditorActionKind::TRANSFORM_EMITTER,
                         .index = index,
                         .emitter = next,
                         .emitter_prior = prior});
    state.selection = {EditorSelectionKind::EMITTER, index};
    return agentEdited(agentEmitterPayload(state, index));
  }

  /// Add @p emitter to the document as one undoable edit, and select it.
  AgentResult addEmitter(EditorShellState& state, EditorEmitter emitter) {
    const size_t index = state.document.emitters.size();
    emitter.id = mintEditorEmitterId(state.document);
    performEditorAction(state.history, state.document,
                        {.kind = EditorActionKind::ADD_EMITTER,
                         .index = index,
                         .emitter = emitter});
    state.selection = {EditorSelectionKind::EMITTER, index};
    return agentEdited(agentEmitterPayload(state, index));
  }

  /// The emitter a `set_effect` call names: the selected one, or the one
  /// at its index. Nothing when it names none that is there.
  std::optional<size_t> emitterTarget(const EditorShellState& state,
                                      const json& params) {
    const std::string target = agentStringParam(params, "target").value_or("");
    std::optional<size_t> index;
    if (target == "selection" &&
        state.selection.kind == EditorSelectionKind::EMITTER) {
      index = state.selection.index;
    } else if (target == "emitter") {
      index = agentIndexParam(params, "index");
    }
    return index && *index < state.document.emitters.size() ? index
                                                            : std::nullopt;
  }

}  // namespace

AgentResult runAgentAddEmitter(EditorShellState& state, const json& params) {
  const std::optional<double> x = agentNumberParam(params, "x");
  const std::optional<double> y = agentNumberParam(params, "y");
  const std::string effect =
      agentStringParam(params, "effect")
          .value_or(std::string(EDITOR_EMITTER_DEFAULT_EFFECT));
  if (!x || !y || game::findCombatFxPreset(effect) == nullptr) {
    return agentFailure(AgentStatus::BAD_PARAMS, ADD_EMITTER_USAGE);
  }
  const WorldPoint at{static_cast<float>(*x), static_cast<float>(*y),
                      agentFloatParam(params, "z", EDITOR_EMITTER_DROP_HEIGHT)};
  return addEmitter(state, makeEditorEmitter(effect, at));
}

AgentResult runAgentSetEffect(EditorShellState& state, const json& params) {
  const std::optional<size_t> index = emitterTarget(state, params);
  const std::string effect = agentStringParam(params, "effect").value_or("");
  if (!index || game::findCombatFxPreset(effect) == nullptr) {
    return agentFailure(AgentStatus::BAD_PARAMS, SET_EFFECT_USAGE);
  }
  const EditorEmitter prior = state.document.emitters[*index];
  EditorEmitter next = prior;
  (void)applyEditorEmitterEffect(next, effect);
  if (sameEditorEmitter(prior, next)) {
    return agentOk(agentEmitterPayload(state, *index));
  }
  return recordEmitter(state, *index, prior, next);
}

AgentResult setAgentEmitterField(EditorShellState& state, size_t index,
                                 EditorPropertyField field, float value) {
  if (!editorEmitterHasField(field)) {
    return agentFailure(AgentStatus::BAD_PARAMS,
                        "a particle emitter holds a position, a direction, "
                        "an interval, its burst and its flash; get_selection "
                        "lists every field it has");
  }
  const EditorEmitter prior = state.document.emitters[index];
  EditorEmitter next = prior;
  setEditorEmitterValue(next, field, value);
  if (sameEditorEmitter(prior, next)) {
    return agentOk(agentEmitterPayload(state, index));
  }
  return recordEmitter(state, index, prior, next);
}

AgentResult translateAgentEmitter(EditorShellState& state, size_t index,
                                  const json& params) {
  const EditorEmitter prior = state.document.emitters[index];
  EditorEmitter next = prior;
  next.position.x += agentFloatParam(params, "dx", 0.0f);
  next.position.y += agentFloatParam(params, "dy", 0.0f);
  next.position.z += agentFloatParam(params, "dz", 0.0f);
  if (sameEditorEmitter(prior, next)) {
    return agentOk(agentEmitterPayload(state, index));
  }
  return recordEmitter(state, index, prior, next);
}

std::string agentEmitterPayload(const EditorShellState& state, size_t index) {
  json out = agentEmitterValue(state.document.emitters[index]);
  out["index"] = index;
  return out.dump(2);
}

json agentEffectPresetsJson() {
  json effects = json::array();
  for (const game::CombatFxPreset& preset : game::combatFxPresets()) {
    effects.push_back({{"id", preset.id}, {"name", preset.name}});
  }
  return effects;
}

}  // namespace eng::editor
