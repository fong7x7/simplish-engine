#include "agent-commands.h"

#include "agent-call.h"
#include "agent-json-values.h"

#include <algorithm>
#include <cstddef>
#include <editor/agent/agent-names.h>
#include <editor/agent/agent-state-json.h>
#include <editor/shell/editor-action-ops.h>
#include <editor/shell/editor-asset-scan.h>
#include <editor/shell/editor-behavior-choices.h>
#include <editor/shell/editor-character-choices.h>
#include <editor/shell/editor-entity-id.h>
#include <editor/shell/editor-level-ops.h>
#include <editor/shell/editor-light-ops.h>
#include <editor/shell/editor-menu-availability.h>
#include <editor/shell/editor-placement-clip.h>
#include <editor/shell/editor-player-start-ops.h>
#include <editor/shell/editor-playtest-session.h>
#include <editor/shell/editor-property-ops.h>
#include <engine/input/input-action.h>
#include <engine/input/player-input-builder.h>
#include <game/content/behavior-names.h>
#include <nlohmann/json.hpp>
#include <optional>
#include <string>
#include <utility>

namespace eng::editor {

namespace {

  using nlohmann::json;

  /// The most ticks one `send_input` call may queue: a minute. A bound so a
  /// typo cannot queue a day of input nobody can see the end of.
  constexpr uint64_t MAX_SEND_INPUT_TICKS = 60U * 60U;

  /// What `add_player_start` says when it is called wrongly.
  constexpr std::string_view ADD_PLAYER_START_USAGE =
      "x and y are required, and player, when given, is a number from 1 "
      "to 4";

  /// How many entries the list @p kind names holds.
  size_t documentCount(const EditorDocument& document,
                       EditorSelectionKind kind) {
    return editorListSize(document, kind);
  }

  /// Whether @p field names one of the three rotation angles, which is
  /// what a light does not store.
  bool isRotationField(EditorPropertyField field) {
    return field >= EditorPropertyField::ROTATION_X &&
           field <= EditorPropertyField::ROTATION_Z;
  }

  /// Whether @p field is one a placement stores.
  ///
  /// Read off the panel's own list rather than kept here by hand: a field a
  /// placement gains is then settable by an agent the moment the panel
  /// shows it, which a second list would only be until somebody forgot it.
  bool isPlacementField(EditorPropertyField field) {
    return std::ranges::find(EDITOR_PLACEMENT_FIELDS, field) !=
           std::end(EDITOR_PLACEMENT_FIELDS);
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
    if (target == "player_start") {
      return EditorSelectionKind::PLAYER_START;
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
          "expected target \"placement\", \"light\", \"player_start\" or "
          "\"selection\", with an index for all but the last");
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

  /// One player start as this API reports it, index included.
  std::string playerStartPayload(const EditorShellState& state, size_t index) {
    json out = agentPlayerStartValue(state.document.player_starts[index]);
    out["index"] = index;
    return out.dump(2);
  }

  /// Record a changed player start as one undoable edit, and select it.
  AgentResult recordPlayerStart(EditorShellState& state, size_t index,
                                const EditorPlayerStart& prior,
                                const EditorPlayerStart& next) {
    performEditorAction(state.history, state.document,
                        {.kind = EditorActionKind::TRANSFORM_PLAYER_START,
                         .index = index,
                         .player_start = next,
                         .player_start_prior = prior});
    selectEntry(state, {EditorSelectionKind::PLAYER_START, index});
    return agentEdited(playerStartPayload(state, index));
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
    if (!isPlacementField(field)) {
      return agentFailure(AgentStatus::BAD_PARAMS,
                          "a placement holds a position, a rotation, a "
                          "scale and whether it collides, and nothing "
                          "else");
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
    if (isRotationField(field) || field == EditorPropertyField::PLAYER ||
        field == EditorPropertyField::COLLIDES ||
        field == EditorPropertyField::SCALE) {
      return agentFailure(AgentStatus::BAD_PARAMS,
                          "a light is aimed by its direction, not turned by "
                          "a rotation; it has a range rather than a scale, "
                          "and neither belongs to a player nor collides");
    }
    const EditorLight prior = state.document.lights[index];
    EditorLight next = prior;
    setEditorLightValue(next, field, value);
    if (editorLightValue(next, field) == editorLightValue(prior, field)) {
      return agentOk(lightPayload(state, index));
    }
    return recordLight(state, index, prior, next);
  }

  /// Write one field of a player start.
  AgentResult setPlayerStartField(EditorShellState& state, size_t index,
                                  EditorPropertyField field, float value) {
    if (!editorPlayerStartHasField(field)) {
      return agentFailure(AgentStatus::BAD_PARAMS,
                          "a player start holds a player and a position, "
                          "and nothing else");
    }
    const EditorPlayerStart prior = state.document.player_starts[index];
    EditorPlayerStart next = prior;
    setEditorPlayerStartValue(next, field, value);
    if (editorPlayerStartValue(next, field) ==
        editorPlayerStartValue(prior, field)) {
      return agentOk(playerStartPayload(state, index));
    }
    return recordPlayerStart(state, index, prior, next);
  }

  /// Write one field of whichever entry @p entry names.
  AgentResult setEntryField(EditorShellState& state, EditorSelection entry,
                            EditorPropertyField field, float value) {
    if (entry.kind == EditorSelectionKind::PLACEMENT) {
      return setPlacementField(state, entry.index, field, value);
    }
    if (entry.kind == EditorSelectionKind::PLAYER_START) {
      return setPlayerStartField(state, entry.index, field, value);
    }
    return setLightField(state, entry.index, field, value);
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

  /// Move a player start, as one undoable edit.
  AgentResult translatePlayerStart(EditorShellState& state, size_t index,
                                   const json& params) {
    const EditorPlayerStart prior = state.document.player_starts[index];
    EditorPlayerStart next = prior;
    next.position = shifted(prior.position, params);
    if (samePoint(next.position, prior.position)) {
      return agentOk(playerStartPayload(state, index));
    }
    return recordPlayerStart(state, index, prior, next);
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

  /// Add @p start to the document as one undoable edit, and select it.
  AgentResult addPlayerStart(EditorShellState& state,
                             const EditorPlayerStart& start) {
    const size_t index = state.document.player_starts.size();
    EditorPlayerStart identified = start;
    if (identified.id.empty()) {
      identified.id = mintEditorPlayerStartId(state.document);
    }
    performEditorAction(state.history, state.document,
                        {.kind = EditorActionKind::ADD_PLAYER_START,
                         .index = index,
                         .player_start = identified});
    selectEntry(state, {EditorSelectionKind::PLAYER_START, index});
    return agentEdited(playerStartPayload(state, index));
  }

  /// The player an `add_player_start` call names, or the lowest one with no
  /// start yet when it names none. Nothing when it names something that is
  /// not a number.
  std::optional<uint8_t> playerParam(const EditorShellState& state,
                                     const json& params) {
    if (!params.contains("player")) {
      return nextEditorPlayerSlot(state.document);
    }
    const std::optional<double> player = agentNumberParam(params, "player");
    if (!player) {
      return std::nullopt;
    }
    return clampEditorPlayerSlot(static_cast<float>(*player));
  }

  /// The removed entry an action carries, as this API reports it.
  json removedValue(const EditorAction& action) {
    if (action.kind == EditorActionKind::REMOVE_PLACEMENT) {
      return agentPlacementValue(action.placement);
    }
    return action.kind == EditorActionKind::REMOVE_LIGHT
               ? agentLightValue(action.light)
               : agentPlayerStartValue(action.player_start);
  }

  /// The entry an action is about to remove, as this API reports it.
  /// Built before the removal, because afterwards there is no entry at
  /// that index to report.
  std::string removedPayload(const EditorAction& action) {
    json out = removedValue(action);
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

  /// The player input a `send_input` call describes: sticks quantised as
  /// the keyboard's are, so scripted input is the same bits a person
  /// pressing the same keys would send.
  sim::PlayerInput sentInput(const json& params) {
    sim::PlayerInput input;
    input.move_x =
        input::quantizeInputAxis(agentFloatParam(params, "move_x", 0));
    input.move_y =
        input::quantizeInputAxis(agentFloatParam(params, "move_y", 0));
    input.aim_x = input::quantizeInputAxis(agentFloatParam(params, "aim_x", 0));
    input.aim_y = input::quantizeInputAxis(agentFloatParam(params, "aim_y", 0));
    input.buttons = agentBoolParam(params, "fire").value_or(false)
                        ? input::INPUT_BUTTON_FIRE
                        : 0U;
    return input;
  }

  /// How many ticks a `send_input` call asks for, or nothing when it names
  /// a number outside 1 to `MAX_SEND_INPUT_TICKS`.
  std::optional<uint32_t> sentTicks(const json& params) {
    if (!params.contains("ticks")) {
      return 1U;
    }
    const std::optional<size_t> ticks = agentIndexParam(params, "ticks");
    if (!ticks || *ticks == 0 || *ticks > MAX_SEND_INPUT_TICKS) {
      return std::nullopt;
    }
    return static_cast<uint32_t>(*ticks);
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

AgentResult runAgentAddPlayerStart(EditorShellState& state,
                                   const json& params) {
  const std::optional<double> x = agentNumberParam(params, "x");
  const std::optional<double> y = agentNumberParam(params, "y");
  const std::optional<uint8_t> player = playerParam(state, params);
  if (!x || !y || !player) {
    return agentFailure(AgentStatus::BAD_PARAMS, ADD_PLAYER_START_USAGE);
  }
  return addPlayerStart(
      state, makeEditorPlayerStart(*player, droppedAt(params, *x, *y, 0.0f)));
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
  return setEntryField(state, entry, field, value);
}

namespace {

  /// @p names written as a list a person reads: `idle, walk, run`.
  std::string joinNames(const std::vector<std::string>& names) {
    std::string out;
    for (const std::string& name : names) {
      out += (out.empty() ? "" : ", ") + name;
    }
    return out;
  }

  /// The rig of @p placement's model when that model is a rigged file, or
  /// null for a static model and a placement naming no asset.
  const EditorAsset* riggedAsset(const EditorShellState& state,
                                 const EditorPlacement& placement) {
    if (placement.asset >= state.assets.size()) {
      return nullptr;
    }
    const EditorAsset& asset = state.assets[placement.asset];
    return !asset.shape && isRiggedModelFile(asset.path) ? &asset : nullptr;
  }

  /// A failure naming @p names when @p clip is not among them. An empty
  /// clip is the model's first, which is always among them.
  std::optional<AgentResult> unknownClip(const std::vector<std::string>& names,
                                         const std::string& clip) {
    if (clip.empty() || std::ranges::find(names, clip) != names.end()) {
      return std::nullopt;
    }
    return agentFailure(AgentStatus::NOT_FOUND,
                        "no clip is called that; the model has: " +
                            joinNames(names));
  }

  /// Why @p placement cannot play @p clip, or nothing when it can.
  std::optional<AgentResult> clipProblem(const EditorShellState& state,
                                         const EditorPlacement& placement,
                                         const std::string& clip) {
    const EditorAsset* asset = riggedAsset(state, placement);
    if (asset == nullptr) {
      return agentFailure(AgentStatus::BAD_PARAMS,
                          "that placement's model is not rigged; only a glTF "
                          "model has clips to play");
    }
    const std::vector<std::string> names = editorClipNames(asset->rig.get());
    if (names.empty()) {
      return agentFailure(AgentStatus::UNAVAILABLE,
                          "that model has no clips loaded — it has none, or "
                          "it failed to load; get_asset says which");
    }
    return unknownClip(names, clip);
  }

}  // namespace

AgentResult runAgentSetAnimation(EditorShellState& state, const json& params) {
  EditorSelection entry{};
  AgentResult resolved = resolveTarget(state, params, entry);
  if (resolved.status != AgentStatus::OK) {
    return resolved;
  }
  if (entry.kind != EditorSelectionKind::PLACEMENT) {
    return agentFailure(AgentStatus::BAD_PARAMS,
                        "only a placement plays clips");
  }
  const EditorPlacement prior = state.document.placements[entry.index];
  const std::string clip = agentStringParam(params, "clip").value_or("");
  if (const auto problem = clipProblem(state, prior, clip)) {
    return *problem;
  }
  if (prior.animation == clip) {
    return agentOk(placementPayload(state, entry.index));
  }
  EditorPlacement next = prior;
  next.animation = clip;
  return recordPlacement(state, entry.index, prior, next);
}

namespace {

  /// Whether @p character is the one @p name refers to: its id, its
  /// `character:` reference, or its display name.
  bool characterMatches(const game::CharacterDefinition& character,
                        const std::string& name) {
    return character.id == name || editorCharacterRef(character.id) == name ||
           character.name == name;
  }

  /// The id of the character the `character` parameter names: empty when
  /// it is omitted or empty, nothing when it names none in the table.
  std::optional<std::string> characterParam(const EditorShellState& state,
                                            const json& params) {
    const std::string name = agentStringParam(params, "character").value_or("");
    if (name.empty()) {
      return std::string{};
    }
    for (const game::CharacterDefinition& character :
         state.characters.characters) {
      if (characterMatches(character, name)) {
        return character.id;
      }
    }
    return std::nullopt;
  }

  /// A failure listing the characters there are to name.
  AgentResult unknownCharacter(const EditorShellState& state) {
    std::vector<std::string> ids;
    for (const game::CharacterDefinition& character :
         state.characters.characters) {
      ids.push_back(character.id);
    }
    return agentFailure(AgentStatus::NOT_FOUND,
                        ids.empty() ? "the project defines no characters; "
                                      "list_characters says where they go"
                                    : "no character is called that; the "
                                      "project has: " +
                                          joinNames(ids));
  }

}  // namespace

AgentResult runAgentSetCharacter(EditorShellState& state, const json& params) {
  EditorSelection entry{};
  AgentResult resolved = resolveTarget(state, params, entry);
  if (resolved.status != AgentStatus::OK) {
    return resolved;
  }
  if (entry.kind != EditorSelectionKind::PLAYER_START) {
    return agentFailure(AgentStatus::BAD_PARAMS,
                        "only a player start has a character");
  }
  const std::optional<std::string> character = characterParam(state, params);
  if (!character) {
    return unknownCharacter(state);
  }
  const std::string ref =
      character->empty() ? std::string{} : editorCharacterRef(*character);
  const EditorPlayerStart prior = state.document.player_starts[entry.index];
  if (prior.character == ref) {
    return agentOk(playerStartPayload(state, entry.index));
  }
  EditorPlayerStart next = prior;
  next.character = ref;
  return recordPlayerStart(state, entry.index, prior, next);
}

namespace {

  /// Whether @p behavior is the one @p name refers to: its id, its
  /// `behavior:` reference, or its display name.
  bool behaviorMatches(const game::BehaviorDefinition& behavior,
                       const std::string& name) {
    return behavior.id == name || editorBehaviorRef(behavior.id) == name ||
           behavior.name == name;
  }

  /// The reference the `behavior` parameter names — empty for an empty
  /// one — or nothing when it names no behavior the project can run.
  std::optional<std::string> behaviorRefNamed(const EditorShellState& state,
                                              const std::string& name) {
    if (name.empty()) {
      return std::string{};
    }
    for (const game::BehaviorDefinition* behavior :
         editorAvailableBehaviors(state.behaviors.behaviors)) {
      if (behaviorMatches(*behavior, name)) {
        return editorBehaviorRef(behavior->id);
      }
    }
    return std::nullopt;
  }

  /// A failure listing the behaviors there are to name.
  AgentResult unknownBehavior(const EditorShellState& state) {
    const auto available = editorAvailableBehaviors(state.behaviors.behaviors);
    std::vector<std::string> ids;
    ids.reserve(available.size());
    for (const game::BehaviorDefinition* behavior : available) {
      ids.push_back(behavior->id);
    }
    return agentFailure(AgentStatus::NOT_FOUND,
                        "no behavior is called that; there are: " +
                            joinNames(ids));
  }

  /// @p next with the `behavior` parameter applied, or the failure it is.
  /// Left out, it keeps the behavior @p next has.
  std::optional<AgentResult> applyBehaviorParam(const EditorShellState& state,
                                                const json& params,
                                                EditorPlacement& next) {
    if (!params.contains("behavior")) {
      return std::nullopt;
    }
    const auto ref = behaviorRefNamed(
        state, agentStringParam(params, "behavior").value_or(""));
    if (!ref) {
      return unknownBehavior(state);
    }
    next.behavior = *ref;
    return std::nullopt;
  }

  /// @p next with the `faction` parameter applied, or the failure it is.
  /// Left out, it keeps the faction @p next has.
  std::optional<AgentResult> applyFactionParam(const json& params,
                                               EditorPlacement& next) {
    if (!params.contains("faction")) {
      return std::nullopt;
    }
    const auto faction =
        game::parseFaction(agentStringParam(params, "faction").value_or(""));
    if (!faction) {
      return agentFailure(AgentStatus::BAD_PARAMS,
                          "faction is \"hostile\", \"neutral\" or "
                          "\"friendly\"");
    }
    next.faction = *faction;
    return std::nullopt;
  }

}  // namespace

AgentResult runAgentSetBehavior(EditorShellState& state, const json& params) {
  EditorSelection entry{};
  AgentResult resolved = resolveTarget(state, params, entry);
  if (resolved.status != AgentStatus::OK) {
    return resolved;
  }
  if (entry.kind != EditorSelectionKind::PLACEMENT) {
    return agentFailure(AgentStatus::BAD_PARAMS,
                        "only a placed prop runs a behavior");
  }
  const EditorPlacement prior = state.document.placements[entry.index];
  EditorPlacement next = prior;
  if (auto problem = applyBehaviorParam(state, params, next)) {
    return *problem;
  }
  if (auto problem = applyFactionParam(params, next)) {
    return *problem;
  }
  if (prior.behavior == next.behavior && prior.faction == next.faction) {
    return agentOk(placementPayload(state, entry.index));
  }
  return recordPlacement(state, entry.index, prior, next);
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
  if (entry.kind == EditorSelectionKind::PLAYER_START) {
    return translatePlayerStart(state, entry.index, params);
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

namespace {

  /// Why a playtest cannot start in @p state, or nothing when it can.
  std::optional<AgentResult> unplayable(const EditorShellState& state) {
    if (!state.project.loaded) {
      return agentFailure(AgentStatus::UNAVAILABLE,
                          "no project is open, so there is no level to play");
    }
    if (state.playtest.mode == EditorPlayMode::PLAYING) {
      return agentFailure(
          AgentStatus::UNAVAILABLE,
          "a playtest is already running; stop_playtest ends it");
    }
    return std::nullopt;
  }

}  // namespace

AgentResult runAgentStartPlaytest(const EditorShellState& state,
                                  const json& params) {
  if (const std::optional<AgentResult> problem = unplayable(state)) {
    return *problem;
  }
  std::optional<std::string> character = characterParam(state, params);
  if (!character) {
    return unknownCharacter(state);
  }
  if (character->empty()) {
    character = editorPlaytestDefaultCharacter(state.document,
                                               state.characters.characters);
  }
  AgentHostRequest request{};
  request.kind = AgentHostRequestKind::START_PLAYTEST;
  request.character = *character;
  return queued(request, "start_playtest");
}

AgentResult runAgentStopPlaytest(const EditorShellState& state) {
  if (state.playtest.mode == EditorPlayMode::EDITING) {
    return agentFailure(AgentStatus::UNAVAILABLE, "no playtest is running");
  }
  return queued(
      {AgentHostRequestKind::RUN_COMMAND, EditorMenuCommand::PLAYTEST, {}, {}},
      "stop_playtest");
}

AgentResult runAgentSendInput(EditorShellState& state, const json& params) {
  if (state.playtest.mode != EditorPlayMode::PLAYING) {
    return agentFailure(AgentStatus::UNAVAILABLE,
                        "no playtest is running; start_playtest first");
  }
  const std::optional<uint32_t> ticks = sentTicks(params);
  if (!ticks) {
    return agentFailure(AgentStatus::BAD_PARAMS,
                        "ticks is a whole number from 1 to 3600");
  }
  state.playtest.scripted.push_back({sentInput(params), *ticks});
  return agentEdited(agentPlaytestJson(state));
}

}  // namespace eng::editor
