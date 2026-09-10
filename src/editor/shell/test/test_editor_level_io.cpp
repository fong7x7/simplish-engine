#include <catch2/catch_test_macros.hpp>
#include <cstdint>
#include <editor/project/project-ops.h>
#include <editor/shell/editor-entity-id.h>
#include <editor/shell/editor-level-io.h>
#include <editor/shell/editor-light-ops.h>
#include <filesystem>
#include <fstream>
#include <optional>
#include <string>
#include <system_error>

namespace fs = std::filesystem;
using namespace eng::editor;

namespace {

/// A temp directory removed when the test scope exits, so a failing
/// assertion cannot leave state behind for the next run.
class TempDir {
public:
  explicit TempDir(const std::string& label) {
    path_ = fs::temp_directory_path() /
            ("simplish-level-" + label + "-" +
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

/// Shell state holding a real project on disk, one asset, and one prop of
/// it standing beside one light.
EditorShellState stateWithProject(const fs::path& root) {
  EditorShellState state;
  auto created = createProject(root, "Saved Project", "2026-09-09T00:00:00Z");
  REQUIRE(created.ok());
  state.project = created.context;
  state.assets.resize(1);
  state.assets[0].name = "Crate";
  state.assets[0].relative_path = "props/crate.obj";
  assignEditorAssetIds(state.assets);
  state.document.placements.push_back(
      {"props_crate_01", 0, {6.0f, 7.0f, 0.0f}, {0.0f, 0.0f, 45.0f}});
  state.document.lights.push_back(
      makeEditorLight(EditorLightKind::POINT, {1.0f, 2.0f, 3.0f}));
  state.document.lights[0].id = "point_01";
  return state;
}

/// Save @p state, then read it back the way a fresh session would: the same
/// project and the same scan, with nothing in the document yet.
EditorLevelLoad reopen(const EditorShellState& state) {
  REQUIRE(saveEditorLevel(state));
  EditorShellState reopened;
  reopened.project = state.project;
  reopened.assets = state.assets;
  std::optional<EditorLevelLoad> load = loadEditorLevel(reopened);
  REQUIRE(load.has_value());
  return *load;
}

}  // namespace

TEST_CASE("saveEditorLevel writes where the project format says") {
  TempDir tmp("write");
  const fs::path root = tmp.path() / "project";
  const EditorShellState state = stateWithProject(root);

  REQUIRE(!editorLevelExists(state));
  REQUIRE(saveEditorLevel(state));
  REQUIRE(editorLevelExists(state));
  REQUIRE(editorLevelPath(root, "main") ==
          root / "content" / "levels" / "main.level.json");
  REQUIRE(fs::is_regular_file(editorLevelPath(state)));
}

TEST_CASE("a saved prop comes back where it stood") {
  TempDir tmp("prop");
  const EditorLevelLoad load = reopen(stateWithProject(tmp.path() / "p"));

  REQUIRE(load.dropped_props == 0);
  REQUIRE(load.document.placements.size() == 1);
  REQUIRE(load.document.placements[0].id == "props_crate_01");
  REQUIRE(load.document.placements[0].asset == 0);
  REQUIRE(load.document.placements[0].position.x == 6.0f);
  REQUIRE(load.document.placements[0].rotation.z == 45.0f);
}

TEST_CASE("a saved light comes back shining the same way") {
  TempDir tmp("light");
  const EditorLevelLoad load = reopen(stateWithProject(tmp.path() / "p"));

  REQUIRE(load.document.lights.size() == 1);
  REQUIRE(load.document.lights[0].id == "point_01");
  REQUIRE(load.document.lights[0].kind == EditorLightKind::POINT);
  REQUIRE(load.document.lights[0].position.y == 2.0f);
}

TEST_CASE("saving twice leaves one file, not two levels") {
  TempDir tmp("twice");
  const fs::path root = tmp.path() / "project";
  EditorShellState state = stateWithProject(root);
  REQUIRE(saveEditorLevel(state));

  state.document.placements.clear();
  REQUIRE(saveEditorLevel(state));

  const std::optional<EditorLevelLoad> load = loadEditorLevel(state);
  REQUIRE(load.has_value());
  REQUIRE(load->document.placements.empty());
  REQUIRE(load->document.lights.size() == 1);
}

TEST_CASE("a project with no level open at an empty document") {
  TempDir tmp("absent");
  const fs::path root = tmp.path() / "project";
  const EditorShellState state = stateWithProject(root);

  REQUIRE(!editorLevelExists(state));
  REQUIRE(!loadEditorLevel(state));
}

TEST_CASE("an unreadable level is refused rather than half-read") {
  TempDir tmp("broken");
  const fs::path root = tmp.path() / "project";
  const EditorShellState state = stateWithProject(root);
  REQUIRE(saveEditorLevel(state));

  std::ofstream out(editorLevelPath(state), std::ios::trunc);
  out << "{ this is not json";
  out.close();

  REQUIRE(editorLevelExists(state));
  REQUIRE(!loadEditorLevel(state));
}

TEST_CASE("saving needs a project to save into") {
  const EditorShellState state;
  REQUIRE(!saveEditorLevel(state));
  REQUIRE(!loadEditorLevel(state));
  REQUIRE(!editorLevelExists(state));
}
