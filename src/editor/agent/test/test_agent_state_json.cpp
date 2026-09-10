#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>
#include <editor/agent/agent-dispatch.h>
#include <editor/agent/agent-state-json.h>
#include <editor/shell/editor-action-ops.h>
#include <editor/shell/editor-general-section.h>
#include <nlohmann/json.hpp>

using Catch::Approx;
using nlohmann::json;
using namespace eng;
using namespace eng::editor;

TEST_CASE("state reports no project as no project, not as an empty name") {
  const EditorShellState state;

  const json reported = json::parse(agentStateJson(state));

  REQUIRE(reported.at("project").at("open") == false);
  REQUIRE(reported.at("project").at("name").is_null());
  REQUIRE(reported.at("selection").at("target") == "none");
  REQUIRE(reported.at("can_undo") == false);
}

TEST_CASE("state reports the camera the viewport last had") {
  EditorShellState state;
  state.view.camera.zoom = 2.0f;
  state.view.show_grid = false;
  state.view.hovered = true;
  state.view.hovered_tile = {4.0f, 5.0f, 0.0f};

  const json camera = json::parse(agentStateJson(state)).at("camera");

  REQUIRE(camera.at("zoom") == Approx(2.0f));
  REQUIRE(camera.at("show_grid") == false);
  REQUIRE(camera.at("hovered_tile").at("x") == Approx(4.0f));
}

TEST_CASE("a pointer outside the viewport hovers no tile at all") {
  const EditorShellState state;

  REQUIRE(json::parse(agentStateJson(state))
              .at("camera")
              .at("hovered_tile")
              .is_null());
}

TEST_CASE("an asset reports how far its mesh and its picture got") {
  EditorShellState state;
  state.assets.push_back({.name = "crate",
                          .path = "/p/assets/props/crate.obj",
                          .relative_path = "props/crate.obj"});
  state.assets[0].load_failed = true;
  state.assets[0].thumbnail_state = EditorAssetThumbnailState::FAILED;

  const json asset = json::parse(agentAssetsJson(state)).at("assets").at(0);

  REQUIRE(asset.at("name") == "crate");
  REQUIRE(asset.at("relative_path") == "props/crate.obj");
  REQUIRE(asset.at("mesh_loaded") == false);
  REQUIRE(asset.at("load_failed") == true);
  REQUIRE(asset.at("thumbnail") == "failed");
  REQUIRE(asset.at("placement_count") == 0);
}

TEST_CASE("an asset counts the placements that instance it") {
  EditorShellState state;
  state.assets.push_back({.name = "crate"});
  state.document.placements.push_back({.position = {0.0f, 0.0f, 0.0f}});
  state.document.placements.push_back({.position = {1.0f, 0.0f, 0.0f}});

  REQUIRE(json::parse(agentAssetsJson(state))
              .at("assets")
              .at(0)
              .at("placement_count") == 2);
}

TEST_CASE("a placement names the asset it instances") {
  EditorShellState state;
  state.assets.push_back({.name = "crate"});
  state.document.placements.push_back(
      {.position = {1.0f, 2.0f, 0.0f}, .rotation = {0, 0, 90}});

  const json placed =
      json::parse(agentPlacementsJson(state)).at("placements").at(0);

  REQUIRE(placed.at("asset_name") == "crate");
  REQUIRE(placed.at("rotation").at("z") == Approx(90.0f));
}

TEST_CASE("a selection lists the rows the properties panel would show") {
  EditorShellState state;
  state.document.lights.push_back({});
  state.selection = {EditorSelectionKind::LIGHT, 0};

  const json selection = json::parse(agentSelectionJson(state));

  REQUIRE(selection.at("target") == "light");
  // A directional light is aimed, not positioned — the panel's own rule.
  REQUIRE(selection.at("fields").at(0).at("name") == "direction_x");
  REQUIRE(selection.at("fields").at(0).at("label") == "Direction X");
}

TEST_CASE("a selection naming an entry that is gone reads as nothing") {
  EditorShellState state;
  state.selection = {EditorSelectionKind::PLACEMENT, 3};

  REQUIRE(json::parse(agentSelectionJson(state)).at("target") == "none");
}

TEST_CASE("history reports the cursor as well as the actions") {
  EditorShellState state;
  (void)runAgentTool(state, "add_light", R"({"kind": "point", "x": 0,
                                             "y": 0})");
  (void)runAgentTool(state, "undo", "{}");

  const json history = json::parse(agentHistoryJson(state));

  REQUIRE(history.at("actions").size() == 1);
  REQUIRE(history.at("actions").at(0).at("kind") == "add_light");
  REQUIRE(history.at("applied") == 0);
  REQUIRE(history.at("can_redo") == true);
}

TEST_CASE("commands say which are built and which would work right now") {
  EditorShellState state;

  const json commands = json::parse(agentCommandsJson(state)).at("commands");
  const auto find = [&commands](std::string_view name) {
    return *std::find_if(
        commands.begin(), commands.end(),
        [name](const json& row) { return row.at("name") == name; });
  };

  REQUIRE(find("save_as").at("implemented") == false);
  REQUIRE(find("close_project").at("implemented") == true);
  // Built, but there is no project to close.
  REQUIRE(find("close_project").at("enabled") == false);
  // Save is built, and needs a project for the same reason.
  REQUIRE(find("save").at("implemented") == true);
  REQUIRE(find("save").at("enabled") == false);
  REQUIRE(find("toggle_grid").at("enabled") == true);
  // The projection is written back to project.json, so with nothing open
  // there is nowhere for the choice to go.
  REQUIRE(find("set_view_isometric").at("implemented") == true);
  REQUIRE(find("set_view_isometric").at("enabled") == false);
}

TEST_CASE("delete_selection is live only while something is selected") {
  EditorShellState state;
  const auto delete_row = [](EditorShellState& s) {
    for (const json& row : json::parse(agentCommandsJson(s)).at("commands")) {
      if (row.at("name") == "delete_selection") {
        return row;
      }
    }
    return json::object();
  };

  REQUIRE(delete_row(state).at("implemented") == true);
  // Built, but there is nothing selected for it to remove.
  REQUIRE(delete_row(state).at("enabled") == false);

  state.document.placements.push_back({});
  state.selection = {EditorSelectionKind::PLACEMENT, 0};
  REQUIRE(delete_row(state).at("enabled") == true);
}

TEST_CASE("the camera reports which projection it is drawing with") {
  EditorShellState state;
  state.project.metadata.projection = ProjectProjection::ISOMETRIC;

  const json camera = json::parse(agentStateJson(state)).at("camera");

  REQUIRE(camera.at("projection") == "isometric");
}

TEST_CASE("folders list the browser's tree and its built-in section") {
  EditorShellState state;
  state.assets.push_back({.name = "crate"});
  appendEditorGeneralSection(state.asset_tree, state.assets.size(),
                             state.assets.size());

  const json folders = json::parse(agentFoldersJson(state)).at("folders");

  REQUIRE(folders.size() > 1);
  REQUIRE(folders.at(EDITOR_ASSET_FOLDER_ROOT).at("parent").is_null());
}

TEST_CASE("describe carries both the manifest and the current state") {
  const EditorShellState state;

  const json described = json::parse(agentDescribeJson(state));

  REQUIRE(described.at("api") == "simplish-editor");
  REQUIRE_FALSE(described.at("tools").empty());
  REQUIRE(described.at("state").at("project").at("open") == false);
  REQUIRE_FALSE(described.at("axes").get<std::string>().empty());
}

TEST_CASE("the level says where it is written and whether it has been") {
  EditorShellState state;
  state.project.loaded = true;
  state.project.root = "/tmp/a-project";

  const json level = json::parse(agentLevelJson(state));

  REQUIRE(level.at("id") == "main");
  REQUIRE(level.at("path") == "/tmp/a-project/content/levels/main.level.json");
  // Nothing has been written there, and nothing needs to be.
  REQUIRE(level.at("on_disk") == false);
  REQUIRE(level.at("readable") == true);
  REQUIRE(level.at("unsaved_changes") == false);
  REQUIRE(level.at("prop_count") == 0);
}

TEST_CASE("the level reports an edit that has not been written") {
  EditorShellState state;
  state.project.loaded = true;
  performEditorAction(state.history, state.document,
                      {.kind = EditorActionKind::PLACE_ASSET, .index = 0});

  const json level = json::parse(agentLevelJson(state));
  REQUIRE(level.at("unsaved_changes") == true);
  REQUIRE(level.at("prop_count") == 1);
  // And at a glance, without a second call.
  REQUIRE(json::parse(agentStateJson(state)).at("unsaved_changes") == true);
}

TEST_CASE("the level of a closed project names no path") {
  const EditorShellState state;

  const json level = json::parse(agentLevelJson(state));
  REQUIRE(level.at("project_open") == false);
  REQUIRE(level.at("path") == "");
}

TEST_CASE("list_levels names every level and which one is open") {
  EditorShellState state;
  state.project.loaded = true;
  state.level_id = "roof";
  state.levels.push_back({"main", true});
  state.levels.push_back({"roof", false});

  const json levels = json::parse(agentLevelsJson(state));

  REQUIRE(levels.at("open") == "roof");
  REQUIRE(levels.at("levels").size() == 2);
  REQUIRE(levels.at("levels")[0].at("id") == "main");
  REQUIRE(levels.at("levels")[0].at("on_disk") == true);
  REQUIRE(levels.at("levels")[0].at("open") == false);
  REQUIRE(levels.at("levels")[1].at("open") == true);
}

TEST_CASE("get_level reports the level being edited, not a fixed one") {
  EditorShellState state;
  state.project.loaded = true;
  state.level_id = "transit_station";

  REQUIRE(json::parse(agentLevelJson(state)).at("id") == "transit_station");
}
