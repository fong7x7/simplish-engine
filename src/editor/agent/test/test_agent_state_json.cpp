#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>
#include <editor/agent/agent-dispatch.h>
#include <editor/agent/agent-state-json.h>
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
  state.document.placements.push_back({0, {0.0f, 0.0f, 0.0f}, {}});
  state.document.placements.push_back({0, {1.0f, 0.0f, 0.0f}, {}});

  REQUIRE(json::parse(agentAssetsJson(state))
              .at("assets")
              .at(0)
              .at("placement_count") == 2);
}

TEST_CASE("a placement names the asset it instances") {
  EditorShellState state;
  state.assets.push_back({.name = "crate"});
  state.document.placements.push_back({0, {1.0f, 2.0f, 0.0f}, {0, 0, 90}});

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

  REQUIRE(find("save").at("implemented") == false);
  REQUIRE(find("close_project").at("implemented") == true);
  // Built, but there is no project to close.
  REQUIRE(find("close_project").at("enabled") == false);
  REQUIRE(find("toggle_grid").at("enabled") == true);
  // The projection is written back to project.json, so with nothing open
  // there is nowhere for the choice to go.
  REQUIRE(find("set_view_isometric").at("implemented") == true);
  REQUIRE(find("set_view_isometric").at("enabled") == false);
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
