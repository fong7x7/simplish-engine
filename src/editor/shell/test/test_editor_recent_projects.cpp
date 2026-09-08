#include <catch2/catch_test_macros.hpp>
#include <cstdint>
#include <editor/shell/simplish-editor.h>
#include <filesystem>
#include <fstream>
#include <string>

namespace fs = std::filesystem;
using namespace eng::editor;

namespace {

/// A temp directory removed when the test scope exits.
class TempDir {
public:
  explicit TempDir(const std::string& label) {
    path_ = fs::temp_directory_path() /
            ("simplish-recent-" + label + "-" +
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

  /// Write a recent-projects file holding two entries.
  fs::path writeList() const {
    const fs::path file = path_ / "recent-projects.json";
    std::ofstream out(file);
    out << R"({"entries":[)"
        << R"({"path":"/one","name":"One","last_opened_at":"2026-01-01T00:00:00Z"},)"
        << R"({"path":"/two","name":"Two","last_opened_at":"2026-01-02T00:00:00Z"}]})";
    return file;
  }

private:
  fs::path path_;
};

}  // namespace

TEST_CASE("the recent list is read as soon as its path is known") {
  const TempDir tmp("load");
  SimplishEditor editor;

  editor.setRecentProjectsPath(tmp.writeList());

  // onInit runs from run(), and a project named on the command line is
  // opened in between — which promotes into this list and saves it. Read
  // any later and the save would land on a list that was never loaded,
  // dropping every entry the file already held.
  REQUIRE(editor.state().recent.entries.size() == 2);
}

TEST_CASE("a missing recent-projects file leaves an empty list") {
  const TempDir tmp("missing");
  SimplishEditor editor;

  editor.setRecentProjectsPath(tmp.path() / "not-here.json");

  // First run on a machine: nothing to read, and nothing to complain about.
  REQUIRE(editor.state().recent.entries.empty());
}

TEST_CASE("an empty path leaves the list alone") {
  SimplishEditor editor;

  editor.setRecentProjectsPath({});

  REQUIRE(editor.state().recent.entries.empty());
  REQUIRE(editor.state().recent_path.empty());
}

TEST_CASE("the path is remembered for the save that follows") {
  const TempDir tmp("path");
  SimplishEditor editor;
  const fs::path file = tmp.writeList();

  editor.setRecentProjectsPath(file);

  REQUIRE(editor.state().recent_path == file);
}
