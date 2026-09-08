#include <algorithm>
#include <catch2/catch_test_macros.hpp>
#include <cstddef>
#include <editor/shell/editor-asset-tree.h>
#include <filesystem>
#include <string>

namespace fs = std::filesystem;
using namespace eng::editor;

namespace {

/// One asset at a relative path, named by its stem, as the scan would
/// produce it. No filesystem is involved: the builder is a pure transform.
EditorAsset makeAsset(const std::string& relative) {
  const fs::path path(relative);
  return {path.stem().string(), fs::path("/project/assets") / path, path};
}

/// The child of @p parent named @p name, or `EDITOR_ASSET_FOLDER_NONE`.
size_t childNamed(const EditorAssetTree& tree, size_t parent,
                  const std::string& name) {
  for (const size_t child : tree.folders[parent].child_folders) {
    if (tree.folders[child].name == name) {
      return child;
    }
  }
  return EDITOR_ASSET_FOLDER_NONE;
}

}  // namespace

TEST_CASE("an empty scan yields a tree holding only the root") {
  const EditorAssetTree tree = buildEditorAssetTree({});

  REQUIRE(tree.folders.size() == 1);
  REQUIRE(tree.folders[EDITOR_ASSET_FOLDER_ROOT].name.empty());
  REQUIRE(tree.folders[EDITOR_ASSET_FOLDER_ROOT].parent ==
          EDITOR_ASSET_FOLDER_NONE);
  REQUIRE(tree.folders[EDITOR_ASSET_FOLDER_ROOT].assets.empty());
  REQUIRE(tree.folders[EDITOR_ASSET_FOLDER_ROOT].child_folders.empty());
}

TEST_CASE("root-level assets hang from the root folder") {
  EditorAssetScan scan;
  scan.assets = {makeAsset("crate.obj"), makeAsset("barrel.obj")};

  const EditorAssetTree tree = buildEditorAssetTree(scan);
  REQUIRE(tree.folders.size() == 1);
  REQUIRE(tree.folders[EDITOR_ASSET_FOLDER_ROOT].assets.size() == 2);
}

TEST_CASE("an asset in a sub-folder creates that folder") {
  EditorAssetScan scan;
  scan.assets = {makeAsset("props/barrel.obj")};

  const EditorAssetTree tree = buildEditorAssetTree(scan);
  const size_t props = childNamed(tree, EDITOR_ASSET_FOLDER_ROOT, "props");
  REQUIRE(props != EDITOR_ASSET_FOLDER_NONE);
  REQUIRE(tree.folders[props].assets.size() == 1);
  REQUIRE(tree.folders[props].relative_path == fs::path("props"));
  // The root holds the folder, not the asset inside it.
  REQUIRE(tree.folders[EDITOR_ASSET_FOLDER_ROOT].assets.empty());
}

TEST_CASE("folder indices address the flat asset list") {
  EditorAssetScan scan;
  scan.assets = {makeAsset("crate.obj"), makeAsset("props/barrel.obj")};

  const EditorAssetTree tree = buildEditorAssetTree(scan);
  const size_t props = childNamed(tree, EDITOR_ASSET_FOLDER_ROOT, "props");
  // Placements index the same list, so a folder must point back into it
  // rather than hold a copy.
  REQUIRE(tree.folders[props].assets.front() == 1);
  REQUIRE(scan.assets[tree.folders[props].assets.front()].name == "barrel");
}

TEST_CASE("intermediate folders are created for a deep asset") {
  EditorAssetScan scan;
  scan.assets = {makeAsset("terrain/rocks/boulder.obj")};

  const EditorAssetTree tree = buildEditorAssetTree(scan);
  const size_t terrain = childNamed(tree, EDITOR_ASSET_FOLDER_ROOT, "terrain");
  REQUIRE(terrain != EDITOR_ASSET_FOLDER_NONE);
  const size_t rocks = childNamed(tree, terrain, "rocks");
  REQUIRE(rocks != EDITOR_ASSET_FOLDER_NONE);
  REQUIRE(tree.folders[rocks].assets.size() == 1);
  REQUIRE(tree.folders[terrain].assets.empty());
}

TEST_CASE("every folder points back at the one holding it") {
  EditorAssetScan scan;
  scan.assets = {makeAsset("terrain/rocks/boulder.obj")};

  const EditorAssetTree tree = buildEditorAssetTree(scan);
  const size_t terrain = childNamed(tree, EDITOR_ASSET_FOLDER_ROOT, "terrain");
  const size_t rocks = childNamed(tree, terrain, "rocks");
  REQUIRE(tree.folders[rocks].parent == terrain);
  REQUIRE(tree.folders[terrain].parent == EDITOR_ASSET_FOLDER_ROOT);
}

TEST_CASE("a scanned folder holding nothing is still a node") {
  EditorAssetScan scan;
  scan.folders = {fs::path("props")};

  const EditorAssetTree tree = buildEditorAssetTree(scan);
  const size_t props = childNamed(tree, EDITOR_ASSET_FOLDER_ROOT, "props");
  REQUIRE(props != EDITOR_ASSET_FOLDER_NONE);
  REQUIRE(tree.folders[props].assets.empty());
}

TEST_CASE("a folder reached from both lists is created once") {
  EditorAssetScan scan;
  scan.folders = {fs::path("props")};
  scan.assets = {makeAsset("props/barrel.obj")};

  const EditorAssetTree tree = buildEditorAssetTree(scan);
  REQUIRE(tree.folders[EDITOR_ASSET_FOLDER_ROOT].child_folders.size() == 1);
  REQUIRE(tree.folders.size() == 2);
}

TEST_CASE("child folders are listed in name order") {
  EditorAssetScan scan;
  scan.folders = {fs::path("zed"), fs::path("alpha"), fs::path("mid")};

  const EditorAssetTree tree = buildEditorAssetTree(scan);
  const auto& children = tree.folders[EDITOR_ASSET_FOLDER_ROOT].child_folders;
  REQUIRE(children.size() == 3);
  REQUIRE(tree.folders[children[0]].name == "alpha");
  REQUIRE(tree.folders[children[1]].name == "mid");
  REQUIRE(tree.folders[children[2]].name == "zed");
}

TEST_CASE("assets within a folder are listed in name order") {
  EditorAssetScan scan;
  // Deliberately out of order: the builder should not rely on the scan
  // having sorted them.
  scan.assets = {makeAsset("props/zed.obj"), makeAsset("props/alpha.obj"),
                 makeAsset("props/mid.obj")};

  const EditorAssetTree tree = buildEditorAssetTree(scan);
  const size_t props = childNamed(tree, EDITOR_ASSET_FOLDER_ROOT, "props");
  const auto& held = tree.folders[props].assets;
  REQUIRE(scan.assets[held[0]].name == "alpha");
  REQUIRE(scan.assets[held[1]].name == "mid");
  REQUIRE(scan.assets[held[2]].name == "zed");
}

TEST_CASE("every asset lands in exactly one folder") {
  EditorAssetScan scan;
  scan.assets = {makeAsset("crate.obj"), makeAsset("props/barrel.obj"),
                 makeAsset("terrain/rocks/boulder.obj")};

  const EditorAssetTree tree = buildEditorAssetTree(scan);
  size_t filed = 0;
  for (const EditorAssetFolder& folder : tree.folders) {
    filed += folder.assets.size();
  }
  REQUIRE(filed == scan.assets.size());
}
