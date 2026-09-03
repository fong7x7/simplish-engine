#include <catch2/catch_test_macros.hpp>
#include <cstdint>
#include <editor/shell/editor-asset-scan.h>
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
            ("simplish-assets-" + label + "-" +
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

  void touch(const std::string& name) const {
    std::ofstream out(path_ / name);
    out << "v 0 0 0\n";
  }

private:
  fs::path path_;
};

}  // namespace

TEST_CASE("obj files in the directory are listed") {
  TempDir tmp("list");
  tmp.touch("crate.obj");
  tmp.touch("barrel.obj");

  const auto assets = scanEditorAssets(tmp.path());
  REQUIRE(assets.size() == 2);
}

TEST_CASE("assets are named by their file stem") {
  TempDir tmp("names");
  tmp.touch("stone_wall.obj");

  const auto assets = scanEditorAssets(tmp.path());
  REQUIRE(assets.front().name == "stone_wall");
  REQUIRE(assets.front().path.filename() == "stone_wall.obj");
}

TEST_CASE("the list is sorted by name") {
  TempDir tmp("sorted");
  tmp.touch("zed.obj");
  tmp.touch("alpha.obj");
  tmp.touch("mid.obj");

  const auto assets = scanEditorAssets(tmp.path());
  // Directory iteration order is not defined, and the panel's card order
  // should not depend on it.
  REQUIRE(assets[0].name == "alpha");
  REQUIRE(assets[1].name == "mid");
  REQUIRE(assets[2].name == "zed");
}

TEST_CASE("uppercase extensions are listed too") {
  TempDir tmp("case");
  tmp.touch("Crate.OBJ");

  REQUIRE(scanEditorAssets(tmp.path()).size() == 1);
}

TEST_CASE("files that are not meshes are ignored") {
  TempDir tmp("other");
  tmp.touch("notes.txt");
  tmp.touch("texture.png");
  tmp.touch("scene.mtl");

  // Listing them as broken assets would be worse than not listing them.
  REQUIRE(scanEditorAssets(tmp.path()).empty());
}

TEST_CASE("an asset starts with no mesh uploaded") {
  TempDir tmp("lazy");
  tmp.touch("crate.obj");

  // Scanning a directory should not parse or upload anything.
  const auto assets = scanEditorAssets(tmp.path());
  REQUIRE(assets.front().mesh == eng::MESH_GPU_INVALID);
  REQUIRE_FALSE(assets.front().load_failed);
}

TEST_CASE("a missing directory yields an empty list") {
  REQUIRE(scanEditorAssets("/nonexistent/assets").empty());
}

TEST_CASE("an empty directory yields an empty list") {
  TempDir tmp("empty");
  REQUIRE(scanEditorAssets(tmp.path()).empty());
}
