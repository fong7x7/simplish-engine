#include <algorithm>
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

  /// Create an empty directory, and any parent it needs.
  void mkdir(const std::string& relative) const {
    std::error_code ec;
    fs::create_directories(path_ / relative, ec);
  }

  /// Write a stub OBJ, creating whatever directories the path implies.
  void touch(const std::string& relative) const {
    const fs::path file = path_ / relative;
    std::error_code ec;
    fs::create_directories(file.parent_path(), ec);
    std::ofstream out(file);
    out << "v 0 0 0\n";
  }

private:
  fs::path path_;
};

/// Whether the scan listed a folder at this relative path.
bool hasFolder(const EditorAssetScan& scan, const std::string& relative) {
  return std::ranges::find(scan.folders, fs::path(relative)) !=
         scan.folders.end();
}

}  // namespace

TEST_CASE("obj files in the directory are listed") {
  TempDir tmp("list");
  tmp.touch("crate.obj");
  tmp.touch("barrel.obj");

  const auto scan = scanEditorAssets(tmp.path());
  REQUIRE(scan.assets.size() == 2);
}

TEST_CASE("gltf and glb models are listed beside obj ones") {
  TempDir tmp("gltf");
  tmp.touch("crate.obj");
  tmp.touch("knight.gltf");
  tmp.touch("wolf.GLB");

  const auto scan = scanEditorAssets(tmp.path());
  REQUIRE(scan.assets.size() == 3);
}

TEST_CASE("only glTF files are rigged models") {
  REQUIRE(isRiggedModelFile("knight.gltf"));
  REQUIRE(isRiggedModelFile("props/wolf.GLB"));
  REQUIRE_FALSE(isRiggedModelFile("crate.obj"));
  REQUIRE_FALSE(isRiggedModelFile("knight.bin"));
}

TEST_CASE("assets are named by their file stem") {
  TempDir tmp("names");
  tmp.touch("stone_wall.obj");

  const auto scan = scanEditorAssets(tmp.path());
  REQUIRE(scan.assets.front().name == "stone_wall");
  REQUIRE(scan.assets.front().path.filename() == "stone_wall.obj");
}

TEST_CASE("the list is sorted by relative path") {
  TempDir tmp("sorted");
  tmp.touch("zed.obj");
  tmp.touch("alpha.obj");
  tmp.touch("mid.obj");

  const auto scan = scanEditorAssets(tmp.path());
  // Directory iteration order is not defined, and the browser's order
  // should not depend on it.
  REQUIRE(scan.assets[0].name == "alpha");
  REQUIRE(scan.assets[1].name == "mid");
  REQUIRE(scan.assets[2].name == "zed");
}

TEST_CASE("uppercase extensions are listed too") {
  TempDir tmp("case");
  tmp.touch("Crate.OBJ");

  REQUIRE(scanEditorAssets(tmp.path()).assets.size() == 1);
}

TEST_CASE("files that are not meshes are ignored") {
  TempDir tmp("other");
  tmp.touch("notes.txt");
  tmp.touch("texture.png");
  tmp.touch("scene.mtl");

  // Listing them as broken assets would be worse than not listing them.
  REQUIRE(scanEditorAssets(tmp.path()).assets.empty());
}

TEST_CASE("an asset starts with no mesh uploaded") {
  TempDir tmp("lazy");
  tmp.touch("crate.obj");

  // Scanning a directory should not parse or upload anything.
  const auto scan = scanEditorAssets(tmp.path());
  REQUIRE(scan.assets.front().mesh == eng::MESH_GPU_INVALID);
  REQUIRE_FALSE(scan.assets.front().load_failed);
}

TEST_CASE("a missing directory yields an empty scan") {
  const auto scan = scanEditorAssets("/nonexistent/assets");
  REQUIRE(scan.assets.empty());
  REQUIRE(scan.folders.empty());
}

TEST_CASE("an empty directory yields an empty scan") {
  TempDir tmp("empty");
  const auto scan = scanEditorAssets(tmp.path());
  REQUIRE(scan.assets.empty());
  REQUIRE(scan.folders.empty());
}

TEST_CASE("assets in sub-directories are listed") {
  TempDir tmp("nested");
  tmp.touch("crate.obj");
  tmp.touch("props/barrel.obj");
  tmp.touch("terrain/rocks/boulder.obj");

  const auto scan = scanEditorAssets(tmp.path());
  REQUIRE(scan.assets.size() == 3);
}

TEST_CASE("an asset records its path relative to the assets root") {
  TempDir tmp("relative");
  tmp.touch("terrain/rocks/boulder.obj");

  const auto scan = scanEditorAssets(tmp.path());
  REQUIRE(scan.assets.front().relative_path ==
          fs::path("terrain") / "rocks" / "boulder.obj");
  // The absolute path still points at the file itself.
  REQUIRE(scan.assets.front().path ==
          tmp.path() / "terrain" / "rocks" / "boulder.obj");
}

TEST_CASE("a root-level asset has a bare relative path") {
  TempDir tmp("relative-root");
  tmp.touch("crate.obj");

  const auto scan = scanEditorAssets(tmp.path());
  REQUIRE(scan.assets.front().relative_path == fs::path("crate.obj"));
  REQUIRE(scan.assets.front().relative_path.parent_path().empty());
}

TEST_CASE("directories are listed alongside the assets") {
  TempDir tmp("folders");
  tmp.touch("props/barrel.obj");
  tmp.mkdir("terrain/rocks");

  const auto scan = scanEditorAssets(tmp.path());
  REQUIRE(hasFolder(scan, "props"));
  REQUIRE(hasFolder(scan, "terrain"));
  REQUIRE(hasFolder(scan, "terrain/rocks"));
}

TEST_CASE("a directory holding no assets is still listed") {
  TempDir tmp("empty-folder");
  tmp.mkdir("props");

  // An empty folder is somewhere to drop a file, so the browser shows it.
  const auto scan = scanEditorAssets(tmp.path());
  REQUIRE(scan.assets.empty());
  REQUIRE(hasFolder(scan, "props"));
}

TEST_CASE("dot-directories are skipped whole") {
  TempDir tmp("hidden");
  tmp.touch(".git/objects/blob.obj");
  tmp.touch(".hidden/crate.obj");
  tmp.touch("props/barrel.obj");

  const auto scan = scanEditorAssets(tmp.path());
  REQUIRE(scan.assets.size() == 1);
  REQUIRE(scan.assets.front().name == "barrel");
  REQUIRE_FALSE(hasFolder(scan, ".git"));
  REQUIRE_FALSE(hasFolder(scan, ".git/objects"));
}

TEST_CASE("assets sort by folder before name") {
  TempDir tmp("nested-sort");
  tmp.touch("props/zed.obj");
  tmp.touch("props/alpha.obj");
  tmp.touch("crate.obj");

  const auto scan = scanEditorAssets(tmp.path());
  // Relative-path order: the root's own file, then the folder's, in name
  // order within it.
  REQUIRE(scan.assets[0].name == "crate");
  REQUIRE(scan.assets[1].name == "alpha");
  REQUIRE(scan.assets[2].name == "zed");
}
