#include <catch2/catch_test_macros.hpp>
#include <cstdint>
#include <editor/project/project-ops.h>
#include <editor/shell/editor-action-ops.h>
#include <editor/shell/editor-entity-id.h>
#include <editor/shell/editor-level-io.h>
#include <editor/shell/editor-level-ops.h>
#include <filesystem>
#include <fstream>
#include <string>
#include <system_error>

namespace fs = std::filesystem;
using namespace eng::editor;

namespace {

/// A temp directory removed when the test scope exits.
class TempDir {
public:
  explicit TempDir(const std::string& label) {
    path_ = fs::temp_directory_path() /
            ("simplish-level-ops-" + label + "-" +
             std::to_string(reinterpret_cast<uintptr_t>(this)));
    std::error_code ec;
    fs::remove_all(path_, ec);
    fs::create_directories(path_, ec);
  }
  ~TempDir() {
    std::error_code ec;
    fs::remove_all(path_, ec);
  }
  TempDir(const TempDir&) = delete;
  TempDir& operator=(const TempDir&) = delete;
  TempDir(TempDir&&) = delete;
  TempDir& operator=(TempDir&&) = delete;

  [[nodiscard]] const fs::path& path() const { return path_; }

private:
  fs::path path_;
};

/// Shell state holding a real project on disk, open at `main`.
EditorShellState stateWithProject(const fs::path& root) {
  EditorShellState state;
  auto created = createProject(root, "Ops Project", "2026-09-09T00:00:00Z");
  REQUIRE(created.ok());
  state.project = created.context;
  // One asset, because a prop names its asset by reference: a level saved
  // against an empty list would drop its props on the way back in.
  state.assets.resize(1);
  state.assets[0].name = "Crate";
  state.assets[0].relative_path = "props/crate.obj";
  assignEditorAssetIds(state.assets);
  return state;
}

/// Place something, so the open level holds an edit no file has.
void placeSomething(EditorShellState& state) {
  performEditorAction(
      state.history, state.document,
      {.kind = EditorActionKind::PLACE_ASSET,
       .index = 0,
       .placement = {"props_crate_01", 0, {6.0f, 7.0f, 0.0f}, {}}});
}

}  // namespace

TEST_CASE("a level id is an identifier the generated code could use") {
  REQUIRE(isEditorLevelId("main"));
  REQUIRE(isEditorLevelId("transit_station_2"));
  REQUIRE(!isEditorLevelId(""));
  REQUIRE(!isEditorLevelId("Main"));
  REQUIRE(!isEditorLevelId("2nd_floor"));
  REQUIRE(!isEditorLevelId("transit station"));
  REQUIRE(!isEditorLevelId("transit-station"));
  REQUIRE(!isEditorLevelId(std::string(EDITOR_LEVEL_ID_MAX + 1, 'a')));
}

TEST_CASE("a typed name becomes the id it was meant to be") {
  REQUIRE(editorLevelIdFromText("Transit Station") == "transit_station");
  REQUIRE(editorLevelIdFromText("  Roof — Top  ") == "roof_top");
  REQUIRE(editorLevelIdFromText("2nd floor") == "nd_floor");
  REQUIRE(editorLevelIdFromText("!!!").empty());
}

TEST_CASE("a created level is written, opened, and listed") {
  TempDir tmp("create");
  const fs::path root = tmp.path() / "project";
  EditorShellState state = stateWithProject(root);

  const EditorLevelResult result =
      createEditorLevel(state, "roof", EditorLevelUnsaved::REFUSE);

  REQUIRE(result.status == EditorLevelStatus::OK);
  REQUIRE(state.level_id == "roof");
  REQUIRE(fs::is_regular_file(editorLevelPath(root, "roof")));
  // Only `roof`: a level exists once its file does, and `main` is the
  // empty level a new project opens at, which nothing has written.
  REQUIRE(state.levels.size() == 1);
  REQUIRE(state.levels[0].id == "roof");
  REQUIRE(!hasUnsavedEditorChanges(state.history));
}

TEST_CASE("a level the project already holds is refused, not emptied") {
  TempDir tmp("dup");
  EditorShellState state = stateWithProject(tmp.path() / "project");
  REQUIRE(createEditorLevel(state, "roof", EditorLevelUnsaved::REFUSE).status ==
          EditorLevelStatus::OK);
  placeSomething(state);
  REQUIRE(saveEditorLevel(state));

  const EditorLevelResult again =
      createEditorLevel(state, "roof", EditorLevelUnsaved::REFUSE);

  REQUIRE(again.status == EditorLevelStatus::ALREADY_EXISTS);
  REQUIRE(state.document.placements.size() == 1);
}

TEST_CASE("an id the format would not accept is refused") {
  TempDir tmp("badid");
  EditorShellState state = stateWithProject(tmp.path() / "project");

  REQUIRE(
      createEditorLevel(state, "Roof Top", EditorLevelUnsaved::REFUSE).status ==
      EditorLevelStatus::INVALID_ID);
  REQUIRE(state.level_id == "main");
}

TEST_CASE("switching level carries nothing of the old one with it") {
  TempDir tmp("switch");
  EditorShellState state = stateWithProject(tmp.path() / "project");
  placeSomething(state);
  REQUIRE(saveEditorLevel(state));
  markEditorChangesSaved(state.history);
  REQUIRE(createEditorLevel(state, "roof", EditorLevelUnsaved::REFUSE).status ==
          EditorLevelStatus::OK);

  REQUIRE(state.document.placements.empty());
  REQUIRE(!canUndoEditorAction(state.history));

  const EditorLevelResult back =
      openEditorLevel(state, "main", EditorLevelUnsaved::REFUSE);

  REQUIRE(back.status == EditorLevelStatus::OK);
  REQUIRE(state.document.placements.size() == 1);
  REQUIRE(!canUndoEditorAction(state.history));
}

/// A project holding `main` and `roof`, open at `main` with one unwritten
/// edit in it.
EditorShellState stateWithTwoLevels(const fs::path& root) {
  EditorShellState state = stateWithProject(root);
  REQUIRE(createEditorLevel(state, "roof", EditorLevelUnsaved::REFUSE).status ==
          EditorLevelStatus::OK);
  REQUIRE(createEditorLevel(state, "main", EditorLevelUnsaved::REFUSE).status ==
          EditorLevelStatus::OK);
  placeSomething(state);
  return state;
}

TEST_CASE("unwritten edits stop a switch") {
  TempDir tmp("unsaved");
  EditorShellState state = stateWithTwoLevels(tmp.path() / "project");

  REQUIRE(openEditorLevel(state, "roof", EditorLevelUnsaved::REFUSE).status ==
          EditorLevelStatus::UNSAVED_CHANGES);
  REQUIRE(state.level_id == "main");
  REQUIRE(state.document.placements.size() == 1);
}

TEST_CASE("a switch that asks to discard them does") {
  TempDir tmp("discard");
  EditorShellState state = stateWithTwoLevels(tmp.path() / "project");

  REQUIRE(openEditorLevel(state, "roof", EditorLevelUnsaved::DISCARD).status ==
          EditorLevelStatus::OK);
  REQUIRE(state.level_id == "roof");
  REQUIRE(state.document.placements.empty());
}

TEST_CASE("a level file that will not parse is opened but not saved over") {
  TempDir tmp("broken");
  const fs::path root = tmp.path() / "project";
  EditorShellState state = stateWithProject(root);
  REQUIRE(createEditorLevel(state, "roof", EditorLevelUnsaved::REFUSE).status ==
          EditorLevelStatus::OK);
  REQUIRE(createEditorLevel(state, "main", EditorLevelUnsaved::REFUSE).status ==
          EditorLevelStatus::OK);
  std::ofstream out(editorLevelPath(root, "roof"), std::ios::trunc);
  out << "{ not json";
  out.close();

  const EditorLevelResult result =
      openEditorLevel(state, "roof", EditorLevelUnsaved::REFUSE);

  REQUIRE(result.status == EditorLevelStatus::UNREADABLE);
  REQUIRE(state.level_id == "roof");
  REQUIRE(!state.level_readable);
}

TEST_CASE("nothing can be created or opened without a project") {
  EditorShellState state;

  REQUIRE(createEditorLevel(state, "roof", EditorLevelUnsaved::REFUSE).status ==
          EditorLevelStatus::NO_PROJECT);
  REQUIRE(openEditorLevel(state, "main", EditorLevelUnsaved::REFUSE).status ==
          EditorLevelStatus::NO_PROJECT);
}

TEST_CASE("a project opens on main, or on the level it does have") {
  TempDir tmp("start");
  const fs::path root = tmp.path() / "project";
  EditorShellState state = stateWithProject(root);
  REQUIRE(createEditorLevel(state, "roof", EditorLevelUnsaved::REFUSE).status ==
          EditorLevelStatus::OK);

  // A project holding only `roof`: opening it lands there rather than on an
  // empty `main` that is not in it.
  EditorShellState reopened;
  reopened.project = state.project;
  reopened.level_id = "whatever_was_open_last";
  chooseEditorStartLevel(reopened);
  REQUIRE(reopened.level_id == "roof");

  REQUIRE(createEditorLevel(state, "main", EditorLevelUnsaved::REFUSE).status ==
          EditorLevelStatus::OK);
  chooseEditorStartLevel(reopened);
  REQUIRE(reopened.level_id == "main");
}
