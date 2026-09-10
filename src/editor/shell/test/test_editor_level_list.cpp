#include <catch2/catch_test_macros.hpp>
#include <cstdint>
#include <editor/project/project-ops.h>
#include <editor/project/project-paths.h>
#include <editor/shell/editor-level-list.h>
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
            ("simplish-level-list-" + label + "-" +
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

/// Write @p name into the project's levels directory, contents unread.
void writeFile(const fs::path& root, const std::string& name) {
  std::ofstream out(projectLevelsPath(root) / name, std::ios::trunc);
  out << "{}";
}

/// Shell state holding a real project on disk, open at `main`.
EditorShellState stateWithProject(const fs::path& root) {
  EditorShellState state;
  auto created = createProject(root, "Listed", "2026-09-09T00:00:00Z");
  REQUIRE(created.ok());
  state.project = created.context;
  return state;
}

}  // namespace

TEST_CASE("levels come back in id order, whatever the filesystem's is") {
  TempDir tmp("order");
  const fs::path root = tmp.path() / "project";
  const EditorShellState state = stateWithProject(root);
  writeFile(root, "roof.level.json");
  writeFile(root, "atrium.level.json");
  writeFile(root, "main.level.json");

  const std::vector<EditorLevelEntry> levels = scanEditorLevels(state);

  REQUIRE(levels.size() == 3);
  REQUIRE(levels[0].id == "atrium");
  REQUIRE(levels[1].id == "main");
  REQUIRE(levels[2].id == "roof");
}

TEST_CASE("only level files are levels") {
  TempDir tmp("filter");
  const fs::path root = tmp.path() / "project";
  const EditorShellState state = stateWithProject(root);
  writeFile(root, "roof.level.json");
  writeFile(root, "notes.txt");
  writeFile(root, "roof.json");
  writeFile(root, ".level.json");

  const std::vector<EditorLevelEntry> levels = scanEditorLevels(state);

  REQUIRE(levels.size() == 2);
  REQUIRE(levels[0].id == "main");
  REQUIRE(levels[1].id == "roof");
}

TEST_CASE("the open level is listed even before it has a file") {
  TempDir tmp("unsaved");
  const EditorShellState state = stateWithProject(tmp.path() / "project");

  const std::vector<EditorLevelEntry> levels = scanEditorLevels(state);

  REQUIRE(levels.size() == 1);
  REQUIRE(levels[0].id == "main");
  REQUIRE(!levels[0].on_disk);
  REQUIRE(hasEditorLevel(state, "main"));
  REQUIRE(!hasEditorLevel(state, "roof"));
}

TEST_CASE("with no project open there are no levels") {
  const EditorShellState state;

  REQUIRE(scanEditorLevels(state).empty());
  REQUIRE(!hasEditorLevel(state, "main"));
}
