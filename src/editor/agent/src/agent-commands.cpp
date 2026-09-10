#include "agent-commands.h"

#include "agent-call.h"
#include "agent-json-values.h"

#include <cstddef>
#include <editor/agent/agent-names.h>
#include <editor/agent/agent-state-json.h>
#include <editor/shell/editor-action-ops.h>
#include <editor/shell/editor-entity-id.h>
#include <editor/shell/editor-level-ops.h>
#include <editor/shell/editor-light-ops.h>
#include <editor/shell/editor-menu-availability.h>
#include <editor/shell/editor-property-ops.h>
#include <nlohmann/json.hpp>
#include <optional>
#include <string>
#include <utility>

namespace eng::editor {

namespace {

  using nlohmann::json;

  /// How many entries the list @p kind names holds.
  size_t documentCount(const EditorDocument& document,
                       EditorSelectionKind kind) {
    if (kind == EditorSelectionKind::PLACEMENT) {
      return document.placements.size();
    }
    return kind == EditorSelectionKind::LIGHT ? document.lights.size() : 0;
  }

  /// Whether @p field names one of the three rotation angles, which is
  /// what a light does not store.
  bool isRotationField(EditorPropertyField field) {
    return field >= EditorPropertyField::ROTATION_X &&
           field <= EditorPropertyField::ROTATION_Z;
  }

  /// Whether @p field names something only a light stores — a direction, a
  /// tint, a strength, a reach.
  bool isLightOnlyField(EditorPropertyField field) {
    return field > EditorPropertyField::ROTATION_Z;
  }

  /// Select @p selection, dropping it when it names an entry that is not
  /// there. The same rule the editor's own `select` applies.
  void selectEntry(EditorShellState& state, EditorSelection selection) {
    const size_t count = documentCount(state.document, selection.kind);
    state.selection = selection.index < count ? selection : EditorSelection{};
  }

  /// Whether an entry named by target and index is actually there.
  AgentResult entryInRange(const EditorShellState& state,
                           const EditorSelection& entry) {
    if (entry.kind == EditorSelectionKind::NONE) {
      return agentFailure(AgentStatus::UNAVAILABLE,
                          "nothing is selected; name a target and an index");
    }
    if (entry.index >= documentCount(state.document, entry.kind)) {
      return agentFailure(AgentStatus::NOT_FOUND,
                          "no entry at that index in that list");
    }
    return agentOk("{}");
  }

  /// The list a target word names.
  std::optional<EditorSelectionKind> targetKind(std::string_view target) {
    if (target == "placement") {
      return EditorSelectionKind::PLACEMENT;
    }
    if (target == "light") {
      return EditorSelectionKind::LIGHT;
    }
    return std::nullopt;
  }

  /// Turn the target and index parameters into one entry of the document.
  AgentResult resolveTarget(const EditorShellState& state, const json& params,
                            EditorSelection& entry) {
    const std::string target = agentStringParam(params, "target").value_or("");
    if (target == "selection") {
      entry = state.selection;
      return entryInRange(state, entry);
    }
    const std::optional<EditorSelectionKind> kind = targetKind(target);
    const std::optional<size_t> index = agentIndexParam(params, "index");
    if (!kind || !index) {
      return agentFailure(
          AgentStatus::BAD_PARAMS,
          "expected target \"placement\", \"light\" or \"selection\", "
          "with an index for the first two");
    }
    entry = {*kind, *index};
    return entryInRange(state, entry);
  }

  /// Whether @p asset is the one @p name refers to: its display name, or
  /// its path relative to the assets root, either of which is something a
  /// person or a previous `list_assets` might have handed the agent.
  bool assetMatches(const EditorAsset& asset, const std::string& name) {
    // The id first, and the qualified reference with it: those are what a
    // level file names an asset by, so they are what an agent reading one
    // will have to hand.
    return asset.id == name || editorAssetRef(asset) == name ||
           asset.name == name || asset.relative_path.generic_string() == name ||
           asset.path.generic_string() == name;
  }

  /// The asset the `asset` parameter names, by index or by name.
  std::optional<size_t> findAsset(const EditorShellState& state,
                                  const json& params) {
    if (const std::optional<size_t> index = agentIndexParam(params, "asset")) {
      return *index < state.assets.size() ? index : std::nullopt;
    }
    const std::optional<std::string> name = agentStringParam(params, "asset");
    if (!name) {
      return std::nullopt;
    }
    for (size_t i = 0; i < state.assets.size(); ++i) {
      if (assetMatches(state.assets[i], *name)) {
        return i;
      }
    }
    return std::nullopt;
  }

  /// One placement as this API reports it, index included.
  std::string placementPayload(const EditorShellState& state, size_t index) {
    json out = agentPlacementValue(state.document.placements[index]);
    out["index"] = index;
    return out.dump(2);
  }

  /// One light as this API reports it, index included.
  std::string lightPayload(const EditorShellState& state, size_t index) {
    json out = agentLightValue(state.document.lights[index]);
    out["index"] = index;
    return out.dump(2);
  }

  /// Record a changed placement as one undoable edit, and select it.
  AgentResult recordPlacement(EditorShellState& state, size_t index,
                              const EditorPlacement& prior,
                              const EditorPlacement& next) {
    performEditorAction(state.history, state.document,
                        {.kind = EditorActionKind::TRANSFORM_PLACEMENT,
                         .index = index,
                         .placement = next,
                         .prior = prior});
    selectEntry(state, {EditorSelectionKind::PLACEMENT, index});
    return agentEdited(placementPayload(state, index));
  }

  /// Record a changed light as one undoable edit, and select it.
  AgentResult recordLight(EditorShellState& state, size_t index,
                          const EditorLight& prior, const EditorLight& next) {
    performEditorAction(state.history, state.document,
                        {.kind = EditorActionKind::TRANSFORM_LIGHT,
                         .index = index,
                         .light = next,
                         .light_prior = prior});
    selectEntry(state, {EditorSelectionKind::LIGHT, index});
    return agentEdited(lightPayload(state, index));
  }

  /// Write one field of a placement.
  AgentResult setPlacementField(EditorShellState& state, size_t index,
                                EditorPropertyField field, float value) {
    if (isLightOnlyField(field)) {
      return agentFailure(AgentStatus::BAD_PARAMS,
                          "a placement holds a position and a rotation; "
                          "that property belongs to a light");
    }
    const EditorPlacement prior = state.document.placements[index];
    EditorPlacement next = prior;
    setEditorPropertyValue(next, field, value);
    if (editorPropertyValue(next, field) == editorPropertyValue(prior, field)) {
      return agentOk(placementPayload(state, index));
    }
    return recordPlacement(state, index, prior, next);
  }

  /// Write one field of a light.
  AgentResult setLightField(EditorShellState& state, size_t index,
                            EditorPropertyField field, float value) {
    if (isRotationField(field)) {
      return agentFailure(AgentStatus::BAD_PARAMS,
                          "a light is aimed by its direction, not turned by "
                          "a rotation");
    }
    const EditorLight prior = state.document.lights[index];
    EditorLight next = prior;
    setEditorLightValue(next, field, value);
    if (editorLightValue(next, field) == editorLightValue(prior, field)) {
      return agentOk(lightPayload(state, index));
    }
    return recordLight(state, index, prior, next);
  }

  /// Whether two positions are the same to the last bit, which is the test
  /// for "this move moved nothing" and so records nothing.
  bool samePoint(WorldPoint a, WorldPoint b) {
    return a.x == b.x && a.y == b.y && a.z == b.z;
  }

  /// @p point shifted by the call's dx, dy and dz, each defaulting to none.
  WorldPoint shifted(WorldPoint point, const json& params) {
    return {point.x + agentFloatParam(params, "dx", 0.0f),
            point.y + agentFloatParam(params, "dy", 0.0f),
            point.z + agentFloatParam(params, "dz", 0.0f)};
  }

  /// Move a placement, as one undoable edit.
  AgentResult translatePlacement(EditorShellState& state, size_t index,
                                 const json& params) {
    const EditorPlacement prior = state.document.placements[index];
    EditorPlacement next = prior;
    next.position = shifted(prior.position, params);
    if (samePoint(next.position, prior.position)) {
      return agentOk(placementPayload(state, index));
    }
    return recordPlacement(state, index, prior, next);
  }

  /// Move a light, as one undoable edit.
  ///
  /// Both kinds move. A directional light shades the same wherever it
  /// stands, so this only moves its marker — but the marker is what somebody
  /// clicks, and refusing to move it would leave it somewhere nothing could
  /// put it.
  AgentResult translateLight(EditorShellState& state, size_t index,
                             const json& params) {
    const EditorLight prior = state.document.lights[index];
    EditorLight next = prior;
    next.position = shifted(prior.position, params);
    if (samePoint(next.position, prior.position)) {
      return agentOk(lightPayload(state, index));
    }
    return recordLight(state, index, prior, next);
  }

  /// The x, y and z the call names, with @p ground standing in for a z it
  /// leaves out. Both placing tools read a position the same way.
  WorldPoint droppedAt(const json& params, double x, double y, float ground) {
    return {static_cast<float>(x), static_cast<float>(y),
            agentFloatParam(params, "z", ground)};
  }

  /// Why an asset cannot be placed, or an OK result when it can.
  AgentResult placeableAsset(const EditorShellState& state,
                             const std::optional<size_t>& asset) {
    if (!asset) {
      return agentFailure(AgentStatus::NOT_FOUND,
                          "no such asset; list_assets reports what this "
                          "project has");
    }
    if (state.assets[*asset].load_failed) {
      return agentFailure(AgentStatus::UNAVAILABLE,
                          "that asset failed to load and cannot be placed");
    }
    return agentOk("{}");
  }

  /// The light kind and the position an `add_light` call names, or the
  /// reason it names neither.
  AgentResult lightKindOf(const json& params,
                          std::optional<EditorLightKind>& kind) {
    const std::optional<std::string> name = agentStringParam(params, "kind");
    kind = name ? findAgentLightKind(*name) : std::nullopt;
    if (!kind) {
      return agentFailure(AgentStatus::BAD_PARAMS,
                          "kind must be \"directional\" or \"point\"");
    }
    return agentOk("{}");
  }

  /// The field and the value a `set_property` call names, or the reason it
  /// names neither.
  AgentResult propertyOf(const json& params, EditorPropertyField& field,
                         float& value) {
    const std::optional<std::string> name = agentStringParam(params, "field");
    const std::optional<EditorPropertyField> found =
        name ? findAgentPropertyField(*name) : std::nullopt;
    const std::optional<double> given = agentNumberParam(params, "value");
    if (!found || !given) {
      return agentFailure(AgentStatus::BAD_PARAMS,
                          "field must name a property and value must be a "
                          "number; get_selection lists the field names");
    }
    field = *found;
    value = static_cast<float>(*given);
    return agentOk("{}");
  }

  /// Add @p placement to the document as one undoable edit, and select it.
  AgentResult addPlacement(EditorShellState& state,
                           const EditorPlacement& placement) {
    // Appended, so undo takes it off the end and redo puts it back here.
    const size_t index = state.document.placements.size();
    EditorPlacement identified = placement;
    // Minted here rather than by the caller, so no route into the document
    // can leave a placement nothing is able to name.
    if (identified.id.empty() && identified.asset < state.assets.size()) {
      identified.id =
          mintEditorPlacementId(state.document, state.assets[identified.asset]);
    }
    performEditorAction(state.history, state.document,
                        {.kind = EditorActionKind::PLACE_ASSET,
                         .index = index,
                         .placement = identified});
    selectEntry(state, {EditorSelectionKind::PLACEMENT, index});
    return agentEdited(placementPayload(state, index));
  }

  /// Add @p light to the document as one undoable edit, and select it.
  AgentResult addLight(EditorShellState& state, const EditorLight& light) {
    const size_t index = state.document.lights.size();
    EditorLight identified = light;
    if (identified.id.empty()) {
      identified.id = mintEditorLightId(state.document, identified.kind);
    }
    performEditorAction(state.history, state.document,
                        {.kind = EditorActionKind::ADD_LIGHT,
                         .index = index,
                         .light = identified});
    selectEntry(state, {EditorSelectionKind::LIGHT, index});
    return agentEdited(lightPayload(state, index));
  }

  /// The entry an action is about to remove, as this API reports it.
  /// Built before the removal, because afterwards there is no entry at
  /// that index to report.
  std::string removedPayload(const EditorAction& action) {
    json out = action.kind == EditorActionKind::REMOVE_PLACEMENT
                   ? agentPlacementValue(action.placement)
                   : agentLightValue(action.light);
    out["index"] = action.index;
    out["removed"] = true;
    return out.dump(2);
  }

  /// Take @p action's entry out of the document as one undoable edit, and
  /// move the selection off it.
  AgentResult removeEntry(EditorShellState& state, const EditorAction& action) {
    const std::string payload = removedPayload(action);
    performEditorAction(state.history, state.document, action);
    selectEntry(state, editorSelectionAfterRedo(action, state.selection));
    return agentEdited(payload);
  }

  /// A tool that has left work for the editor: what was queued, and the
  /// request the editor drains on the tick that ran this.
  AgentResult queued(AgentHostRequest request, std::string_view what) {
    AgentResult result = agentEdited(json{{"queued", what}}.dump(2));
    result.host = std::move(request);
    return result;
  }

  /// How a level refusal reads to a caller: a bad id is a parameter to
  /// fix, a level that is not there is a name to re-read, and everything
  /// else is a state to change first.
  AgentStatus agentStatusForLevel(EditorLevelStatus status) {
    switch (status) {
      case EditorLevelStatus::INVALID_ID:
        return AgentStatus::BAD_PARAMS;
      case EditorLevelStatus::NOT_FOUND:
        return AgentStatus::NOT_FOUND;
      case EditorLevelStatus::OK:
      case EditorLevelStatus::NO_PROJECT:
      case EditorLevelStatus::ALREADY_EXISTS:
      case EditorLevelStatus::UNSAVED_CHANGES:
      case EditorLevelStatus::WRITE_FAILED:
      case EditorLevelStatus::UNREADABLE:
        return AgentStatus::UNAVAILABLE;
    }
    return AgentStatus::UNAVAILABLE;
  }

  /// What a call asked to happen to unwritten edits: refusing by default,
  /// and nothing at all for a word that is neither.
  std::optional<EditorLevelUnsaved> levelUnsavedParam(const json& params) {
    const std::optional<std::string> word = agentStringParam(params, "unsaved");
    if (word) {
      return findAgentLevelUnsaved(*word);
    }
    return params.contains("unsaved")
               ? std::nullopt
               : std::optional{EditorLevelUnsaved::REFUSE};
  }

  /// Whether the editor would carry this request out, asked before it is
  /// queued so the caller is told why rather than watching nothing happen.
  EditorLevelStatus levelRequestCheck(const EditorShellState& state,
                                      std::string_view id,
                                      EditorLevelUnsaved unsaved,
                                      AgentHostRequestKind kind) {
    return kind == AgentHostRequestKind::CREATE_LEVEL
               ? canCreateEditorLevel(state, id, unsaved)
               : canOpenEditorLevel(state, id, unsaved);
  }

  /// Both level tools, which differ only in the question they ask.
  AgentResult queueLevelRequest(const EditorShellState& state,
                                const json& params, AgentHostRequestKind kind) {
    const std::optional<std::string> id = agentStringParam(params, "id");
    const std::optional<EditorLevelUnsaved> unsaved = levelUnsavedParam(params);
    if (!id || !unsaved) {
      return agentFailure(AgentStatus::BAD_PARAMS,
                          "id is required, and unsaved is \"refuse\" or "
                          "\"discard\"");
    }
    const EditorLevelStatus check =
        levelRequestCheck(state, *id, *unsaved, kind);
    if (check != EditorLevelStatus::OK) {
      return agentFailure(agentStatusForLevel(check),
                          editorLevelStatusMessage(check));
    }
    return queued({kind, EditorMenuCommand::SEPARATOR, {}, *id, *unsaved}, *id);
  }

}  // namespace

AgentResult runAgentGetAsset(EditorShellState& state, const json& params) {
  const std::optional<size_t> index = findAsset(state, params);
  if (!index) {
    return agentFailure(AgentStatus::NOT_FOUND,
                        "no such asset; list_assets reports what this "
                        "project has");
  }
  return agentOk(agentAssetJson(state, *index));
}

AgentResult runAgentPlaceAsset(EditorShellState& state, const json& params) {
  const std::optional<double> x = agentNumberParam(params, "x");
  const std::optional<double> y = agentNumberParam(params, "y");
  if (!x || !y) {
    return agentFailure(AgentStatus::BAD_PARAMS, "x and y are required");
  }
  const std::optional<size_t> asset = findAsset(state, params);
  const AgentResult placeable = placeableAsset(state, asset);
  if (placeable.status != AgentStatus::OK) {
    return placeable;
  }
  EditorPlacement placement{};
  placement.asset = *asset;
  placement.position = droppedAt(params, *x, *y, 0.0f);
  return addPlacement(state, placement);
}

AgentResult runAgentAddLight(EditorShellState& state, const json& params) {
  std::optional<EditorLightKind> kind;
  const AgentResult named = lightKindOf(params, kind);
  if (named.status != AgentStatus::OK) {
    return named;
  }
  const std::optional<double> x = agentNumberParam(params, "x");
  const std::optional<double> y = agentNumberParam(params, "y");
  if (!x || !y) {
    return agentFailure(AgentStatus::BAD_PARAMS, "x and y are required");
  }
  return addLight(state,
                  makeEditorLight(*kind, droppedAt(params, *x, *y,
                                                   EDITOR_LIGHT_DROP_HEIGHT)));
}

AgentResult runAgentSetProperty(EditorShellState& state, const json& params) {
  EditorSelection entry{};
  const AgentResult resolved = resolveTarget(state, params, entry);
  if (resolved.status != AgentStatus::OK) {
    return resolved;
  }
  EditorPropertyField field{};
  float value = 0.0f;
  const AgentResult named = propertyOf(params, field, value);
  if (named.status != AgentStatus::OK) {
    return named;
  }
  if (entry.kind == EditorSelectionKind::PLACEMENT) {
    return setPlacementField(state, entry.index, field, value);
  }
  return setLightField(state, entry.index, field, value);
}

AgentResult runAgentTranslate(EditorShellState& state, const json& params) {
  EditorSelection entry{};
  const AgentResult resolved = resolveTarget(state, params, entry);
  if (resolved.status != AgentStatus::OK) {
    return resolved;
  }
  if (entry.kind == EditorSelectionKind::PLACEMENT) {
    return translatePlacement(state, entry.index, params);
  }
  return translateLight(state, entry.index, params);
}

AgentResult runAgentDelete(EditorShellState& state, const json& params) {
  EditorSelection entry{};
  AgentResult resolved = resolveTarget(state, params, entry);
  if (resolved.status != AgentStatus::OK) {
    return resolved;
  }
  // Built from the document rather than from the target, so the editor's
  // Delete key and this tool remove an entry by the same description of
  // what a removal is — and undo puts back the same one either way.
  const std::optional<EditorAction> action =
      editorDeleteAction(state.document, entry);
  if (!action) {
    return agentFailure(AgentStatus::NOT_FOUND,
                        "no entry at that index in that list");
  }
  return removeEntry(state, *action);
}

AgentResult runAgentSelect(EditorShellState& state, const json& params) {
  if (agentStringParam(params, "target").value_or("") == "none") {
    state.selection = EditorSelection{};
    return agentEdited(agentSelectionJson(state));
  }
  EditorSelection entry{};
  const AgentResult resolved = resolveTarget(state, params, entry);
  if (resolved.status != AgentStatus::OK) {
    return resolved;
  }
  selectEntry(state, entry);
  return agentEdited(agentSelectionJson(state));
}

AgentResult runAgentSetTool(EditorShellState& state, const json& params) {
  const std::optional<std::string> name = agentStringParam(params, "tool");
  const std::optional<EditorTool> tool =
      name ? findAgentEditorTool(*name) : std::nullopt;
  if (!tool) {
    return agentFailure(AgentStatus::BAD_PARAMS,
                        "no such tool; get_state lists the tool names");
  }
  state.active_tool = *tool;
  return agentEdited(json{{"active_tool", agentEditorToolName(*tool)}}.dump(2));
}

AgentResult runAgentUndo(EditorShellState& state) {
  if (!canUndoEditorAction(state.history)) {
    return agentFailure(AgentStatus::UNAVAILABLE, "nothing to undo");
  }
  // Read before the cursor moves: this is the action about to be reverted,
  // and it is what says where the selection lands.
  const EditorAction action = state.history.actions[state.history.applied - 1];
  (void)undoEditorAction(state.history, state.document);
  selectEntry(state, editorSelectionAfterUndo(action, state.selection));
  return agentEdited(agentHistoryJson(state));
}

AgentResult runAgentRedo(EditorShellState& state) {
  if (!canRedoEditorAction(state.history)) {
    return agentFailure(AgentStatus::UNAVAILABLE, "nothing to redo");
  }
  const EditorAction action = state.history.actions[state.history.applied];
  (void)redoEditorAction(state.history, state.document);
  selectEntry(state, editorSelectionAfterRedo(action, state.selection));
  return agentEdited(agentHistoryJson(state));
}

AgentResult runAgentRunCommand(const EditorShellState& state,
                               const json& params) {
  const std::optional<std::string> name = agentStringParam(params, "command");
  const std::optional<EditorMenuCommand> command =
      name ? findAgentMenuCommand(*name) : std::nullopt;
  if (!command) {
    return agentFailure(AgentStatus::BAD_PARAMS,
                        "no such command; list_commands reports every one");
  }
  if (!editorMenuCommandEnabled(state, *command)) {
    return agentFailure(AgentStatus::UNAVAILABLE,
                        "that command is disabled right now; list_commands "
                        "says which are live");
  }
  return queued({AgentHostRequestKind::RUN_COMMAND, *command, {}, {}}, *name);
}

AgentResult runAgentOpenProject(const json& params) {
  const std::string path = agentStringParam(params, "path").value_or("");
  if (path.empty()) {
    return agentFailure(AgentStatus::BAD_PARAMS,
                        "path is required: the directory holding the "
                        "project");
  }
  return queued({AgentHostRequestKind::OPEN_PROJECT,
                 EditorMenuCommand::SEPARATOR,
                 path,
                 {}},
                "open_project");
}

AgentResult runAgentRescanAssets(const EditorShellState& state) {
  if (!state.project.loaded) {
    return agentFailure(AgentStatus::UNAVAILABLE,
                        "no project is open, so there is nothing to rescan");
  }
  return queued({AgentHostRequestKind::RESCAN_ASSETS,
                 EditorMenuCommand::SEPARATOR,
                 {},
                 {}},
                "rescan_assets");
}

AgentResult runAgentCreateLevel(const EditorShellState& state,
                                const json& params) {
  return queueLevelRequest(state, params, AgentHostRequestKind::CREATE_LEVEL);
}

AgentResult runAgentOpenLevel(const EditorShellState& state,
                              const json& params) {
  return queueLevelRequest(state, params, AgentHostRequestKind::OPEN_LEVEL);
}

}  // namespace eng::editor
