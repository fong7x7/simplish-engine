#include <catch2/catch_test_macros.hpp>
#include <editor/agent/agent-dispatch.h>
#include <editor/shell/editor-action-ops.h>
#include <nlohmann/json.hpp>
#include <string>

using nlohmann::json;
using namespace eng::editor;

namespace {

/// A project holding two sprite sheets and nothing in its level yet.
EditorShellState openProject() {
  EditorShellState state;
  state.project.loaded = true;
  state.sheets = {std::filesystem::path("sprites/slime.png"),
                  std::filesystem::path("props/torch.png")};
  return state;
}

/// Run one tool and give back its parsed payload.
json call(EditorShellState& state, std::string_view tool,
          std::string_view params) {
  const AgentResult result = runAgentTool(state, tool, params);
  INFO("tool " << tool << " said " << result.json);
  REQUIRE(result.status == AgentStatus::OK);
  return json::parse(result.json);
}

/// The status one tool call ends in.
AgentStatus statusOf(EditorShellState& state, std::string_view tool,
                     std::string_view params) {
  return runAgentTool(state, tool, params).status;
}

}  // namespace

TEST_CASE("add_sprite stands a billboard on the floor and selects it") {
  EditorShellState state = openProject();
  const json added = call(state, "add_sprite", R"({"x": 2.5, "y": 3.5})");

  REQUIRE(state.document.sprites.size() == 1);
  REQUIRE(added["id"] == "sprite_01");
  REQUIRE(added["ref"] == "sprite:sprite_01");
  // Feet on the ground, which is where a billboard's depth is measured.
  REQUIRE(added["position"]["z"] == 0.0);
  // The project's first sheet, so a drop into a project with art shows
  // some of it at once.
  REQUIRE(added["sheet"] == "sprites/slime.png");
  REQUIRE(state.selection.kind == EditorSelectionKind::SPRITE);
  REQUIRE(hasUnsavedEditorChanges(state.history));
}

TEST_CASE("add_sprite takes the grid and the speed of its sheet") {
  EditorShellState state = openProject();
  const json added =
      call(state, "add_sprite",
           R"({"x": 0, "y": 0, "sheet": "props/torch.png", "columns": 4,
          "rows": 3, "frames": 10, "fps": 8, "height": 2})");

  REQUIRE(added["sheet"] == "props/torch.png");
  REQUIRE(added["properties"]["columns"] == 4.0);
  REQUIRE(added["properties"]["frames"] == 10.0);
  REQUIRE(added["properties"]["fps"] == 8.0);
  REQUIRE(added["properties"]["height"] == 2.0);
}

TEST_CASE("add_sprite refuses a sheet the project does not hold") {
  EditorShellState state = openProject();

  REQUIRE(statusOf(state, "add_sprite",
                   R"({"x": 0, "y": 0, "sheet": "gone/ghost.png"})") ==
          AgentStatus::BAD_PARAMS);
  REQUIRE(state.document.sprites.empty());
}

TEST_CASE("a project with no sheets still places a billboard") {
  EditorShellState state;
  state.project.loaded = true;
  const json added = call(state, "add_sprite", R"({"x": 1, "y": 1})");

  // Nothing to show yet, and the Sheet row is what fixes that: refusing
  // the drop would make a billboard unplaceable until art arrived.
  REQUIRE(added["sheet"] == "");
  REQUIRE(state.document.sprites.size() == 1);
}

TEST_CASE("set_sheet repoints a billboard and keeps its grid") {
  EditorShellState state = openProject();
  call(state, "add_sprite",
       R"({"x": 0, "y": 0, "columns": 4, "rows": 2, "fps": 6})");
  const json changed =
      call(state, "set_sheet",
           R"({"target": "selection", "sheet": "props/torch.png"})");

  REQUIRE(changed["sheet"] == "props/torch.png");
  REQUIRE(changed["properties"]["columns"] == 4.0);
  REQUIRE(changed["properties"]["fps"] == 6.0);
}

TEST_CASE("set_sheet refuses a sheet the project does not hold") {
  EditorShellState state = openProject();
  call(state, "add_sprite", R"({"x": 0, "y": 0})");

  REQUIRE(statusOf(state, "set_sheet",
                   R"({"target": "sprite", "index": 0,
                       "sheet": "gone/ghost.png"})") ==
          AgentStatus::BAD_PARAMS);
  REQUIRE(state.document.sprites[0].sheet == "sprites/slime.png");
}

TEST_CASE("set_property writes a billboard's grid through the panel's rule") {
  EditorShellState state = openProject();
  call(state, "add_sprite", R"({"x": 0, "y": 0, "columns": 4, "rows": 3})");
  const json written =
      call(state, "set_property",
           R"({"target": "sprite", "index": 0, "field": "frames",
               "value": 99})");

  // Held to the cells the grid has, and the response says what was stored.
  REQUIRE(written["properties"]["frames"] == 12.0);
}

TEST_CASE("set_property refuses a field a billboard has not got") {
  EditorShellState state = openProject();
  call(state, "add_sprite", R"({"x": 0, "y": 0})");

  REQUIRE(statusOf(state, "set_property",
                   R"({"target": "sprite", "index": 0, "field": "intensity",
                       "value": 2})") == AgentStatus::BAD_PARAMS);
}

TEST_CASE("translate moves a billboard, and an undo puts it back") {
  EditorShellState state = openProject();
  call(state, "add_sprite", R"({"x": 2, "y": 2})");
  call(state, "translate",
       R"({"target": "sprite", "index": 0, "dx": 1.5, "dz": 0.25})");

  REQUIRE(state.document.sprites[0].position.x == 3.5f);
  REQUIRE(state.document.sprites[0].position.z == 0.25f);

  call(state, "undo", "{}");
  REQUIRE(state.document.sprites[0].position.x == 2.0f);
}

TEST_CASE("delete takes a billboard out, and reports the one it took") {
  EditorShellState state = openProject();
  call(state, "add_sprite", R"({"x": 2, "y": 2})");
  const json removed =
      call(state, "delete", R"({"target": "sprite", "index": 0})");

  REQUIRE(state.document.sprites.empty());
  REQUIRE(removed["removed"] == true);
  REQUIRE(removed["id"] == "sprite_01");

  // Undo puts back the billboard that was there, not a fresh one.
  call(state, "undo", "{}");
  REQUIRE(state.document.sprites.size() == 1);
  REQUIRE(state.document.sprites[0].id == "sprite_01");
}

TEST_CASE("list_sprites reports the level's billboards and the sheets") {
  EditorShellState state = openProject();
  call(state, "add_sprite", R"({"x": 1, "y": 1})");
  const json listed = call(state, "list_sprites", "{}");

  REQUIRE(listed["sprites"].size() == 1);
  REQUIRE(listed["sprites"][0]["index"] == 0);
  REQUIRE(listed["sheets"].size() == 2);
  REQUIRE(listed["sheets"][0] == "sprites/slime.png");
}

TEST_CASE("get_selection shows a billboard's rows, Sheet row and all") {
  EditorShellState state = openProject();
  call(state, "add_sprite", R"({"x": 1, "y": 1})");
  const json selected = call(state, "get_selection", "{}");

  REQUIRE(selected["target"] == "sprite");
  REQUIRE(selected["name"] == "Sprite Billboard · sprites/slime");
  REQUIRE(selected["fields"].size() == 8);
}
