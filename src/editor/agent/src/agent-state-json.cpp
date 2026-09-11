#include "agent-json-values.h"

#include <algorithm>
#include <cstddef>
#include <editor/agent/agent-names.h>
#include <editor/agent/agent-state-json.h>
#include <editor/agent/agent-tool-info.h>
#include <editor/shell/editor-action-ops.h>
#include <editor/shell/editor-asset-scan.h>
#include <editor/shell/editor-behavior-choices.h>
#include <editor/shell/editor-behavior-table.h>
#include <editor/shell/editor-character-choices.h>
#include <editor/shell/editor-entity-id.h>
#include <editor/shell/editor-general-item.h>
#include <editor/shell/editor-level-io.h>
#include <editor/shell/editor-level-json.h>
#include <editor/shell/editor-light-ops.h>
#include <editor/shell/editor-menu-availability.h>
#include <editor/shell/editor-navigation.h>
#include <editor/shell/editor-placement-clip.h>
#include <editor/shell/editor-player-start-ops.h>
#include <editor/shell/editor-property-ops.h>
#include <editor/shell/editor-property-traits.h>
#include <editor/shell/editor-waypoint-ops.h>
#include <game/content/behavior-lookup.h>
#include <game/content/behavior-names.h>
#include <nlohmann/json.hpp>
#include <span>
#include <string>

namespace eng::editor {

namespace {

  using nlohmann::json;

  /// What the editor build calls itself. Bumped when the tool surface
  /// changes in a way a caller written against the old one would notice.
  constexpr int AGENT_API_VERSION = 1;

  /// The open project, or nulls that say plainly that there is none.
  json projectJson(const EditorShellState& state) {
    if (!state.project.loaded) {
      return {{"open", false}, {"name", nullptr}, {"root", nullptr}};
    }
    return {{"open", true},
            {"name", state.project.metadata.name},
            {"root", state.project.root.generic_string()}};
  }

  /// Where the viewport camera sits and what it is over.
  json cameraJson(const EditorShellState& state) {
    const EditorViewState& view = state.view;
    json out = {
        {"focus_x", view.camera.focus.x},
        {"focus_y", view.camera.focus.y},
        {"zoom", view.camera.zoom},
        {"show_grid", view.show_grid},
        {"show_navigation", view.show_navigation},
        {"show_ai", view.show_ai},
        // Named rather than derived from the axes: an agent asking
        // which projection it is looking at wants the word the
        // project file and the View menu both use.
        {"projection",
         projectProjectionName(state.project.metadata.projection)},
        // The same word for the look: "smooth" or "cel".
        {"shading", projectShadingName(state.project.metadata.shading)}};
    out["hovered_tile"] =
        view.hovered ? agentPointJson(view.hovered_tile) : json(nullptr);
    return out;
  }

  /// Every toolbar tool, so a caller knows what `set_tool` will accept
  /// without going back to the manifest.
  json toolNamesJson() {
    json names = json::array();
    for (EditorTool tool : EDITOR_TOOLS) {
      names.push_back(agentEditorToolName(tool));
    }
    return names;
  }

  /// How many placements instance the asset at @p index.
  size_t placementsOfAsset(const EditorShellState& state, size_t index) {
    size_t count = 0;
    for (const EditorPlacement& placement : state.document.placements) {
      count += placement.asset == index ? 1 : 0;
    }
    return count;
  }

  /// One asset and everything the editor knows about its state.
  json assetValue(const EditorShellState& state, size_t index) {
    const EditorAsset& asset = state.assets[index];
    return {{"index", index},
            {"id", asset.id},
            {"ref", editorAssetRef(asset)},
            {"name", asset.name},
            {"path", asset.path.generic_string()},
            {"relative_path", asset.relative_path.generic_string()},
            {"mesh_loaded", editorAssetLoaded(asset)},
            {"rigged", !asset.shape && isRiggedModelFile(asset.path)},
            {"clips", editorClipNames(asset.rig.get())},
            {"load_failed", asset.load_failed},
            {"thumbnail", agentThumbnailStateName(asset.thumbnail_state)},
            {"bounds",
             {{"min", agentVec3Json(asset.min)},
              {"max", agentVec3Json(asset.max)}}},
            {"placement_count", placementsOfAsset(state, index)}};
  }

  /// One entry a folder holds: a scanned asset, or a built-in item
  /// numbered after them.
  json entryValue(const EditorShellState& state, size_t entry) {
    if (entry < state.assets.size()) {
      return {{"entry", entry},
              {"kind", "asset"},
              {"id", state.assets[entry].id},
              {"name", state.assets[entry].name}};
    }
    const size_t item = entry - state.assets.size();
    if (item >= EDITOR_GENERAL_ITEM_COUNT) {
      return {{"entry", entry}, {"kind", "unknown"}, {"name", ""}};
    }
    return {{"entry", entry},
            {"kind", "builtin"},
            {"name", editorGeneralItemName(EDITOR_GENERAL_ITEMS[item])}};
  }

  /// One folder of the browser's tree.
  json folderValue(const EditorShellState& state, size_t index) {
    const EditorAssetFolder& folder = state.asset_tree.folders[index];
    json entries = json::array();
    for (size_t entry : folder.assets) {
      entries.push_back(entryValue(state, entry));
    }
    json out = {{"index", index},
                {"name", folder.name},
                {"relative_path", folder.relative_path.generic_string()},
                {"child_folders", folder.child_folders},
                {"entries", entries}};
    out["parent"] = folder.parent == EDITOR_ASSET_FOLDER_NONE
                        ? json(nullptr)
                        : json(folder.parent);
    return out;
  }

  /// The asset a placement names, or an empty name when the list no longer
  /// has it — the same thing the properties panel shows for one.
  std::string placementAssetName(const EditorShellState& state, size_t asset) {
    return asset < state.assets.size() ? state.assets[asset].name
                                       : std::string{};
  }

  /// One recorded edit, named by what it did and what it did it to.
  json actionValue(const EditorAction& action, size_t index) {
    return {{"index", index},
            {"kind", agentActionKindName(action.kind)},
            {"target_index", action.index}};
  }

  /// One menu command, and whether it would do anything right now.
  json commandValue(const EditorShellState& state,
                    const EditorMenuCommandInfo& info) {
    return {{"name", agentMenuCommandName(info.command)},
            {"label", info.label},
            {"shortcut", info.shortcut},
            {"implemented", editorMenuCommandImplemented(info.command)},
            {"enabled", editorMenuCommandEnabled(state, info.command)}};
  }

  /// One field row: what it is called, what the panel labels it, and what
  /// it currently holds.
  json fieldValue(EditorPropertyField field, float value) {
    return {{"name", agentPropertyFieldName(field)},
            {"label", editorPropertyFieldLabel(field)},
            {"value", value}};
  }

  /// The rows the panel lists for a placement.
  json placementFields(const EditorPlacement& placement) {
    json out = json::array();
    for (EditorPropertyField field : EDITOR_PLACEMENT_FIELDS) {
      out.push_back(fieldValue(field, editorPropertyValue(placement, field)));
    }
    return out;
  }

  /// The rows the panel lists for a light, which depend on its kind.
  json lightFields(const EditorLight& light) {
    json out = json::array();
    for (EditorPropertyField field : editorLightFields(light.kind)) {
      out.push_back(fieldValue(field, editorLightValue(light, field)));
    }
    return out;
  }

  /// The rows the panel lists for a player start.
  json playerStartFields(const EditorPlayerStart& start) {
    json out = json::array();
    for (EditorPropertyField field : EDITOR_PLAYER_START_FIELDS) {
      out.push_back(fieldValue(field, editorPlayerStartValue(start, field)));
    }
    return out;
  }

  /// The selected waypoint, as the panel shows it.
  json waypointSelectionJson(const EditorShellState& state, size_t index) {
    const EditorWaypoint& waypoint = state.document.waypoints[index];
    json out = agentWaypointValue(waypoint);
    out["target"] = "waypoint";
    out["index"] = index;
    out["name"] = editorWaypointName(waypoint);
    json fields = json::array();
    for (EditorPropertyField field : EDITOR_WAYPOINT_FIELDS) {
      fields.push_back(fieldValue(field, editorWaypointValue(waypoint, field)));
    }
    out["fields"] = std::move(fields);
    return out;
  }

  /// The ids of the actors in @p document patrolling @p route.
  json patrolledBy(const EditorDocument& document, uint8_t route) {
    json ids = json::array();
    for (const EditorPlacement& placement : document.placements) {
      if (!placement.behavior.empty() && placement.route == route) {
        ids.push_back(placement.id);
      }
    }
    return ids;
  }

  /// One route of @p document: where it goes, in walking order, and who
  /// walks it.
  json routeJson(const EditorDocument& document, uint8_t route) {
    json points = json::array();
    for (const Vec2 point : editorRoutePoints(document, route)) {
      points.push_back({{"x", point.x}, {"y", point.y}});
    }
    return {{"route", route},
            {"points", std::move(points)},
            {"patrolled_by", patrolledBy(document, route)}};
  }

  /// The selected placement, as the panel shows it.
  json placementSelectionJson(const EditorShellState& state, size_t index) {
    const EditorPlacement& placement = state.document.placements[index];
    json out = agentPlacementValue(placement);
    out["target"] = "placement";
    out["index"] = index;
    out["asset_name"] = placementAssetName(state, placement.asset);
    out["fields"] = placementFields(placement);
    return out;
  }

  /// The selected light, as the panel shows it.
  json lightSelectionJson(const EditorShellState& state, size_t index) {
    const EditorLight& light = state.document.lights[index];
    json out = agentLightValue(light);
    out["target"] = "light";
    out["index"] = index;
    out["fields"] = lightFields(light);
    return out;
  }

  /// The selected player start, as the panel shows it.
  json playerStartSelectionJson(const EditorShellState& state, size_t index) {
    const EditorPlayerStart& start = state.document.player_starts[index];
    json out = agentPlayerStartValue(start);
    out["target"] = "player_start";
    out["index"] = index;
    out["name"] = editorPlayerStartName(start);
    out["fields"] = playerStartFields(start);
    return out;
  }

  /// What is selected, as the panel shows it. Only asked of a selection
  /// naming an entry that is there.
  json selectedEntryJson(const EditorShellState& state) {
    const size_t index = state.selection.index;
    switch (state.selection.kind) {
      case EditorSelectionKind::PLACEMENT:
        return placementSelectionJson(state, index);
      case EditorSelectionKind::LIGHT:
        return lightSelectionJson(state, index);
      case EditorSelectionKind::PLAYER_START:
        return playerStartSelectionJson(state, index);
      case EditorSelectionKind::WAYPOINT:
        return waypointSelectionJson(state, index);
      case EditorSelectionKind::NONE:
        break;
    }
    return {{"target", agentSelectionKindName(EditorSelectionKind::NONE)}};
  }

  /// A 64-bit hash as sixteen hex digits. As a JSON number it would be
  /// rounded by any reader that holds numbers as doubles, which is most of
  /// them, and a hash that is almost right is no hash at all.
  std::string hashHex(uint64_t hash) {
    constexpr std::string_view DIGITS = "0123456789abcdef";
    std::string out(16, '0');
    for (size_t i = 16; i > 0; --i) {
      out[i - 1] = DIGITS[hash & 0xFU];
      hash >>= 4U;
    }
    return out;
  }

  /// Every player in a playtest, as the agent API reports them.
  json playtestPlayersJson(const EditorPlaytestState& playtest) {
    json players = json::array();
    for (const EditorPlaytestPlayer& player : playtest.players) {
      players.push_back({{"player", player.player},
                         {"position", agentPointJson(player.position)},
                         {"character", player.character},
                         {"health", player.health},
                         {"max_health", player.max_health},
                         {"downed", player.downed},
                         {"out", player.out},
                         {"stand_in", player.stand_in}});
    }
    return players;
  }

  /// One actor in a playtest, as the agent API reports it.
  json playtestActorJson(const EditorPlaytestActor& actor) {
    return {{"id", actor.id},
            {"position", agentPointJson(actor.position)},
            {"facing", {{"x", actor.facing.x}, {"y", actor.facing.y}}},
            {"behavior", actor.behavior},
            {"state", actor.state},
            {"faction", game::factionName(actor.faction)},
            {"target_player", actor.target},
            {"target_actor", actor.target_actor},
            {"sees_target", actor.sees_target},
            {"path_waypoints", actor.path_waypoints},
            {"health", actor.health},
            {"max_health", actor.max_health}};
  }

  /// Every actor in a playtest, as the agent API reports them.
  json playtestActorsJson(const EditorPlaytestState& playtest) {
    json actors = json::array();
    for (const EditorPlaytestActor& actor : playtest.actors) {
      actors.push_back(playtestActorJson(actor));
    }
    return actors;
  }

  /// Every projectile and hazard pool in a playtest, as the agent API
  /// reports them.
  json playtestCombatJson(const EditorPlaytestState& playtest) {
    json projectiles = json::array();
    for (const WorldPoint& at : playtest.projectiles) {
      projectiles.push_back(agentPointJson(at));
    }
    json hazards = json::array();
    for (const EditorPlaytestHazard& pool : playtest.hazards) {
      hazards.push_back({{"position", agentPointJson(pool.position)},
                         {"radius", pool.radius},
                         {"ticks_left", pool.ticks_left}});
    }
    return {{"projectiles", projectiles}, {"hazards", hazards}};
  }

  /// Ticks of scripted input still waiting to run.
  uint64_t queuedInputTicks(const EditorPlaytestState& playtest) {
    uint64_t ticks = 0;
    for (const EditorScriptedInput& input : playtest.scripted) {
      ticks += input.ticks;
    }
    return ticks;
  }

  /// One parameter of one tool, as the manifest publishes it.
  json paramValue(const AgentParam& param) {
    return {{"name", param.name},
            {"type", agentParamTypeName(param.type)},
            {"required", param.need == AgentParamNeed::REQUIRED},
            {"description", param.description}};
  }

  /// One tool, as the manifest publishes it.
  json toolValue(const AgentToolInfo& info) {
    json params = json::array();
    for (const AgentParam& param : info.params) {
      params.push_back(paramValue(param));
    }
    return {{"name", info.name},
            {"summary", info.summary},
            {"effect", agentToolEffectName(info.effect)},
            {"params", params}};
  }

}  // namespace

std::string agentStateJson(const EditorShellState& state) {
  json out = {{"project", projectJson(state)},
              {"active_tool", agentEditorToolName(state.active_tool)},
              {"available_tools", toolNamesJson()},
              {"camera", cameraJson(state)},
              {"asset_count", state.assets.size()},
              {"placement_count", state.document.placements.size()},
              {"light_count", state.document.lights.size()},
              {"player_start_count", state.document.player_starts.size()},
              {"waypoint_count", state.document.waypoints.size()},
              {"can_undo", canUndoEditorAction(state.history)},
              {"can_redo", canRedoEditorAction(state.history)},
              {"unsaved_changes", hasUnsavedEditorChanges(state.history)},
              {"playtest", agentPlayModeName(state.playtest.mode)}};
  out["selection"] = json::parse(agentSelectionJson(state));
  return out.dump(2);
}

std::string agentLevelJson(const EditorShellState& state) {
  const bool loaded = state.project.loaded;
  json out = {{"id", state.level_id},
              {"project_open", loaded},
              {"on_disk", editorLevelExists(state)},
              {"readable", state.level_readable},
              {"unsaved_changes", hasUnsavedEditorChanges(state.history)},
              {"prop_count", state.document.placements.size()},
              {"light_count", state.document.lights.size()},
              {"player_start_count", state.document.player_starts.size()},
              {"waypoint_count", state.document.waypoints.size()}};
  // Only where there is a project to be relative to; an absolute path made
  // from an empty root would name the working directory, not a level.
  out["path"] = loaded ? editorLevelPath(state).generic_string() : "";
  return out.dump(2);
}

std::string agentLevelsJson(const EditorShellState& state) {
  json levels = json::array();
  for (const EditorLevelEntry& level : state.levels) {
    levels.push_back({{"id", level.id},
                      {"on_disk", level.on_disk},
                      {"open", level.id == state.level_id}});
  }
  return json{{"levels", std::move(levels)}, {"open", state.level_id}}.dump(2);
}

std::string agentAssetsJson(const EditorShellState& state) {
  json assets = json::array();
  for (size_t i = 0; i < state.assets.size(); ++i) {
    assets.push_back(assetValue(state, i));
  }
  return json{{"assets", assets}}.dump(2);
}

std::string agentAssetJson(const EditorShellState& state, size_t index) {
  return assetValue(state, index).dump(2);
}

std::string agentFoldersJson(const EditorShellState& state) {
  json folders = json::array();
  for (size_t i = 0; i < state.asset_tree.folders.size(); ++i) {
    folders.push_back(folderValue(state, i));
  }
  return json{{"folders", folders}, {"root", EDITOR_ASSET_FOLDER_ROOT}}.dump(2);
}

std::string agentPlacementsJson(const EditorShellState& state) {
  json placements = json::array();
  const auto& list = state.document.placements;
  for (size_t i = 0; i < list.size(); ++i) {
    json entry = agentPlacementValue(list[i]);
    entry["index"] = i;
    entry["asset_name"] = placementAssetName(state, list[i].asset);
    placements.push_back(entry);
  }
  return json{{"placements", placements}}.dump(2);
}

std::string agentLightsJson(const EditorShellState& state) {
  json lights = json::array();
  const auto& list = state.document.lights;
  for (size_t i = 0; i < list.size(); ++i) {
    json entry = agentLightValue(list[i]);
    entry["index"] = i;
    lights.push_back(entry);
  }
  return json{{"lights", lights}}.dump(2);
}

std::string agentPlayerStartsJson(const EditorShellState& state) {
  json starts = json::array();
  const auto& list = state.document.player_starts;
  for (size_t i = 0; i < list.size(); ++i) {
    json entry = agentPlayerStartValue(list[i]);
    entry["index"] = i;
    starts.push_back(entry);
  }
  return json{{"player_starts", starts}, {"player_slots", EDITOR_PLAYER_SLOTS}}
      .dump(2);
}

std::string agentWaypointsJson(const EditorShellState& state) {
  json waypoints = json::array();
  const auto& list = state.document.waypoints;
  for (size_t i = 0; i < list.size(); ++i) {
    json entry = agentWaypointValue(list[i]);
    entry["index"] = i;
    waypoints.push_back(entry);
  }
  json routes = json::array();
  for (const uint8_t route : editorRoutesInUse(state.document)) {
    routes.push_back(routeJson(state.document, route));
  }
  return json{{"waypoints", waypoints},
              {"routes", routes},
              {"route_count", EDITOR_ROUTE_COUNT}}
      .dump(2);
}

std::string agentPlaytestJson(const EditorShellState& state) {
  const EditorPlaytestState& playtest = state.playtest;
  json out = {{"mode", agentPlayModeName(playtest.mode)},
              {"paused", playtest.clock == EditorPlaytestClock::PAUSED},
              {"tick", playtest.tick},
              {"dropped_ticks", playtest.dropped_ticks},
              {"players", playtestPlayersJson(playtest)},
              {"actors", playtestActorsJson(playtest)},
              {"queued_input_ticks", queuedInputTicks(playtest)},
              {"run_over", playtest.run_over},
              {"stand_ins", state.playtest_stand_ins}};
  out.update(playtestCombatJson(playtest));
  out["hash"] = playtest.hash ? json(hashHex(*playtest.hash)) : json(nullptr);
  return out.dump(2);
}

namespace {

  /// One character, as `list_characters` reports it.
  json characterJson(const game::CharacterDefinition& character) {
    return {{"id", character.id},
            {"ref", editorCharacterRef(character.id)},
            {"name", character.name},
            {"model", character.model},
            {"move_speed", character.move_speed},
            {"health", character.health}};
  }

}  // namespace

std::string agentCharactersJson(const EditorShellState& state) {
  json characters = json::array();
  for (const game::CharacterDefinition& character :
       state.characters.characters) {
    characters.push_back(characterJson(character));
  }
  const std::string path =
      state.project.loaded
          ? editorCharacterTablePath(state.project.root).generic_string()
          : std::string{};
  return json{{"file", path},
              {"characters", characters},
              {"problems", state.characters.problems}}
      .dump(2);
}

namespace {

  /// The ids of @p behavior's states, in order.
  json stateIds(const game::BehaviorDefinition& behavior) {
    json ids = json::array();
    for (const game::BehaviorState& state : behavior.states) {
      ids.push_back(state.id);
    }
    return ids;
  }

  /// Whether @p behavior is one of the game's own presets, not a row of
  /// the project's table.
  bool builtIn(const game::BehaviorDefinition& behavior) {
    for (const game::BehaviorDefinition& preset : game::builtInBehaviors()) {
      if (&preset == &behavior) {
        return true;
      }
    }
    return false;
  }

  /// One enemy archetype, as `list_enemies` reports it, with whether
  /// anything @p state can run is the behavior it names.
  json enemyJson(const EditorShellState& state,
                 const game::EnemyDefinition& enemy) {
    const std::string ref = editorBehaviorRef(enemy.behavior);
    return {{"id", enemy.id},
            {"name", enemy.name},
            {"model", enemy.model},
            {"health", enemy.health},
            {"radius", enemy.radius},
            {"height", enemy.height},
            {"behavior", enemy.behavior},
            {"behavior_ref", ref},
            {"behavior_known",
             findEditorBehavior(state.behaviors.behaviors, ref) != nullptr},
            {"faction", game::factionName(enemy.faction)}};
  }

  /// One behavior, as `list_behaviors` reports it.
  json behaviorJson(const game::BehaviorDefinition& behavior) {
    return {{"id", behavior.id},
            {"ref", editorBehaviorRef(behavior.id)},
            {"name", behavior.name},
            {"built_in", builtIn(behavior)},
            {"states", stateIds(behavior)},
            {"initial", behavior.states[behavior.initial].id},
            {"targets", game::behaviorTargetsName(behavior.senses.targets)}};
  }

}  // namespace

std::string agentEnemiesJson(const EditorShellState& state) {
  json enemies = json::array();
  for (const game::EnemyDefinition& enemy : state.enemies.enemies) {
    enemies.push_back(enemyJson(state, enemy));
  }
  const std::string path =
      state.project.loaded
          ? editorEnemyTablePath(state.project.root).generic_string()
          : std::string{};
  return json{{"file", path},
              {"enemies", enemies},
              {"problems", state.enemies.problems}}
      .dump(2);
}

std::string agentBehaviorsJson(const EditorShellState& state) {
  json behaviors = json::array();
  for (const game::BehaviorDefinition* behavior :
       editorAvailableBehaviors(state.behaviors.behaviors)) {
    behaviors.push_back(behaviorJson(*behavior));
  }
  const std::string path =
      state.project.loaded
          ? editorBehaviorTablePath(state.project.root).generic_string()
          : std::string{};
  return json{{"file", path},
              {"behaviors", behaviors},
              {"problems", state.behaviors.problems}}
      .dump(2);
}

namespace {

  /// How many of @p navigation's cells are @p kind.
  size_t countCells(const EditorNavigation& navigation, EditorNavCell kind) {
    return static_cast<size_t>(std::ranges::count(navigation.cells, kind));
  }

  /// The grid's placement and size.
  json gridJson(const spatial::NavGrid& grid) {
    const spatial::NavGridSpec& spec = grid.spec();
    return {{"origin", {{"x", spec.origin.x}, {"y", spec.origin.y}}},
            {"width", spec.width},
            {"height", spec.height},
            {"cell_size", spec.cell_size}};
  }

  /// How many cells are of each kind.
  json cellCountsJson(const EditorNavigation& navigation) {
    return {
        {"open", countCells(navigation, EditorNavCell::OPEN)},
        {"solid", countCells(navigation, EditorNavCell::SOLID)},
        {"narrow", countCells(navigation, EditorNavCell::NARROW)},
        {"unreachable", countCells(navigation, EditorNavCell::UNREACHABLE)}};
  }

}  // namespace

std::string agentNavigationJson(const EditorShellState& state) {
  const EditorNavigation navigation =
      analyseEditorNavigation(state.document, state.assets);
  return json{{"grid", gridJson(navigation.grid)},
              {"clearance", navigation.clearance},
              {"cells", cellCountsJson(navigation)},
              {"reachability_known", navigation.reachability_known},
              {"unreachable_actors", navigation.unreachable_actors},
              {"stranded_actors", navigation.stranded_actors}}
      .dump(2);
}

std::string agentSelectionJson(const EditorShellState& state) {
  const EditorSelection& selection = state.selection;
  if (selection.index >= editorListSize(state.document, selection.kind)) {
    return json{{"target", agentSelectionKindName(EditorSelectionKind::NONE)}}
        .dump(2);
  }
  return selectedEntryJson(state).dump(2);
}

std::string agentHistoryJson(const EditorShellState& state) {
  json actions = json::array();
  const auto& list = state.history.actions;
  for (size_t i = 0; i < list.size(); ++i) {
    actions.push_back(actionValue(list[i], i));
  }
  return json{{"actions", actions},
              {"applied", state.history.applied},
              {"can_undo", canUndoEditorAction(state.history)},
              {"can_redo", canRedoEditorAction(state.history)}}
      .dump(2);
}

std::string agentCommandsJson(const EditorShellState& state) {
  json commands = json::array();
  for (const EditorMenuCommandInfo& info : EDITOR_MENU_COMMAND_INFO) {
    if (info.command != EditorMenuCommand::SEPARATOR) {
      commands.push_back(commandValue(state, info));
    }
  }
  return json{{"commands", commands}}.dump(2);
}

std::string agentManifestJson() {
  json tools = json::array();
  for (AgentTool tool : AGENT_TOOLS) {
    tools.push_back(toolValue(agentToolInfo(tool)));
  }
  return json{{"api", "simplish-editor"},
              {"version", AGENT_API_VERSION},
              {"axes", AGENT_AXIS_NOTE},
              {"tools", tools}}
      .dump(2);
}

std::string agentDescribeJson(const EditorShellState& state) {
  json out = json::parse(agentManifestJson());
  out["state"] = json::parse(agentStateJson(state));
  return out.dump(2);
}

}  // namespace eng::editor
