#include <catch2/catch_test_macros.hpp>
#include <cstdint>
#include <editor/project/project-ops.h>
#include <editor/project/project-paths.h>
#include <filesystem>
#include <fstream>
#include <string>

namespace fs = std::filesystem;
using namespace eng::editor;

namespace {

/// A temp directory removed when the test scope exits, so a failing
/// assertion cannot leave state behind for the next run.
class TempDir {
public:
  explicit TempDir(const std::string& label) {
    path_ = fs::temp_directory_path() /
            ("simplish-editor-test-" + label + "-" +
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

constexpr const char* STAMP = "2026-08-22T12:00:00Z";

}  // namespace

TEST_CASE("createProject writes a manifest and a data directory") {
  TempDir tmp("create");
  const fs::path root = tmp.path() / "my-project";

  auto result = createProject(root, "My Project", STAMP);
  REQUIRE(result.ok());
  REQUIRE(result.context.loaded);
  REQUIRE(result.context.metadata.name == "My Project");
  REQUIRE(result.context.metadata.created_at == STAMP);
  REQUIRE(result.context.metadata.default_workspace == "Level");
  REQUIRE(fs::is_regular_file(projectFilePath(root)));
  REQUIRE(fs::is_directory(projectDataPath(root)));
}

TEST_CASE("createProject refuses to overwrite an existing project") {
  TempDir tmp("no-overwrite");
  const fs::path root = tmp.path() / "existing";

  REQUIRE(createProject(root, "First", STAMP).ok());
  auto second = createProject(root, "Second", STAMP);
  REQUIRE_FALSE(second.ok());
  REQUIRE(second.error == ProjectOpenError::ALREADY_EXISTS);

  // The original manifest is intact.
  auto reopened = openProject(root);
  REQUIRE(reopened.ok());
  REQUIRE(reopened.context.metadata.name == "First");
}

TEST_CASE("openProject reports why it failed") {
  TempDir tmp("failures");

  SECTION("path does not exist") {
    auto result = openProject(tmp.path() / "nope");
    REQUIRE(result.error == ProjectOpenError::PATH_NOT_FOUND);
    REQUIRE_FALSE(result.context.loaded);
  }

  SECTION("path is a file, not a directory") {
    const fs::path file = tmp.path() / "a-file.txt";
    std::ofstream(file) << "x";
    auto result = openProject(file);
    REQUIRE(result.error == ProjectOpenError::NOT_A_DIRECTORY);
  }

  SECTION("directory holds no manifest") {
    const fs::path dir = tmp.path() / "plain-dir";
    fs::create_directories(dir);
    auto result = openProject(dir);
    REQUIRE(result.error == ProjectOpenError::NOT_A_PROJECT);
  }

  SECTION("manifest is not valid JSON") {
    const fs::path dir = tmp.path() / "broken";
    fs::create_directories(projectDirPath(dir));
    std::ofstream(projectFilePath(dir)) << "{ this is not json";
    auto result = openProject(dir);
    REQUIRE(result.error == ProjectOpenError::MALFORMED);
  }
}

TEST_CASE("openProject does not rewrite the manifest") {
  TempDir tmp("read-only-open");
  const fs::path root = tmp.path() / "p";
  REQUIRE(createProject(root, "P", "2026-01-01T00:00:00Z").ok());

  auto opened = openProject(root);
  REQUIRE(opened.ok());
  REQUIRE(opened.context.metadata.last_opened_at == "2026-01-01T00:00:00Z");
}

TEST_CASE("touchProjectOpened stamps last_opened_at and persists it") {
  TempDir tmp("touch");
  const fs::path root = tmp.path() / "p";
  REQUIRE(createProject(root, "P", "2026-01-01T00:00:00Z").ok());

  auto opened = openProject(root);
  REQUIRE(opened.ok());
  REQUIRE(touchProjectOpened(opened.context, STAMP));
  REQUIRE(opened.context.metadata.last_opened_at == STAMP);

  auto reopened = openProject(root);
  REQUIRE(reopened.ok());
  REQUIRE(reopened.context.metadata.last_opened_at == STAMP);
}

TEST_CASE("touchProjectOpened is a no-op with no project loaded") {
  ProjectContext empty;
  REQUIRE_FALSE(touchProjectOpened(empty, STAMP));
}

TEST_CASE("isProjectDirectory recognises only real project roots") {
  TempDir tmp("detect");
  const fs::path root = tmp.path() / "p";
  REQUIRE_FALSE(isProjectDirectory(root));
  REQUIRE(createProject(root, "P", STAMP).ok());
  REQUIRE(isProjectDirectory(root));
}

TEST_CASE("promoteRecentProject moves an existing entry to the front") {
  TempDir tmp("recent-promote");
  const fs::path first = tmp.path() / "first";
  const fs::path second = tmp.path() / "second";
  auto a = createProject(first, "First", STAMP);
  auto b = createProject(second, "Second", STAMP);
  REQUIRE(a.ok());
  REQUIRE(b.ok());

  RecentProjectsList list;
  promoteRecentProject(list, a.context, STAMP);
  promoteRecentProject(list, b.context, STAMP);
  REQUIRE(list.entries.size() == 2);
  REQUIRE(list.entries.front().name == "Second");

  // Re-promoting the older project moves it up without duplicating it.
  promoteRecentProject(list, a.context, STAMP);
  REQUIRE(list.entries.size() == 2);
  REQUIRE(list.entries.front().name == "First");
}

TEST_CASE("promoteRecentProject caps the list") {
  TempDir tmp("recent-cap");
  RecentProjectsList list;

  for (size_t i = 0; i < RECENT_PROJECTS_MAX + 4; ++i) {
    const fs::path root = tmp.path() / ("p" + std::to_string(i));
    auto created = createProject(root, "P" + std::to_string(i), STAMP);
    REQUIRE(created.ok());
    promoteRecentProject(list, created.context, STAMP);
  }
  REQUIRE(list.entries.size() == RECENT_PROJECTS_MAX);
  // The most recently promoted project is still at the front.
  REQUIRE(list.entries.front().name ==
          "P" + std::to_string(RECENT_PROJECTS_MAX + 3));
}

TEST_CASE("promoteRecentProject ignores an unloaded project") {
  RecentProjectsList list;
  ProjectContext empty;
  promoteRecentProject(list, empty, STAMP);
  REQUIRE(list.entries.empty());
}

TEST_CASE("removeRecentProject reports whether it removed anything") {
  RecentProjectsList list;
  list.entries.push_back(RecentProjectEntry{"/a", "A", STAMP});
  REQUIRE(removeRecentProject(list, "/a"));
  REQUIRE(list.entries.empty());
  REQUIRE_FALSE(removeRecentProject(list, "/a"));
}

TEST_CASE("recent projects round-trip through disk") {
  TempDir tmp("recent-io");
  const fs::path file = tmp.path() / "nested" / "recent-projects.json";

  RecentProjectsList list;
  list.entries.push_back(RecentProjectEntry{"/x", "X", STAMP});
  REQUIRE(saveRecentProjects(list, file));

  auto loaded = loadRecentProjects(file);
  REQUIRE(loaded.entries.size() == 1);
  REQUIRE(loaded.entries[0].path == "/x");
}

TEST_CASE("loadRecentProjects treats a missing file as an empty list") {
  TempDir tmp("recent-missing");
  auto loaded = loadRecentProjects(tmp.path() / "absent.json");
  REQUIRE(loaded.entries.empty());
}

TEST_CASE("isoTimestampNow is shaped like an ISO 8601 UTC timestamp") {
  const std::string stamp = isoTimestampNow();
  REQUIRE(stamp.size() == 20);
  REQUIRE(stamp[4] == '-');
  REQUIRE(stamp[7] == '-');
  REQUIRE(stamp[10] == 'T');
  REQUIRE(stamp[13] == ':');
  REQUIRE(stamp[16] == ':');
  REQUIRE(stamp[19] == 'Z');
}
