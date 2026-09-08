#include <catch2/catch_test_macros.hpp>
#include <cstddef>
#include <editor/shell/editor-asset-folder-rows.h>
#include <filesystem>
#include <string>
#include <unordered_set>

namespace fs = std::filesystem;
using namespace eng::editor;

namespace {

/// An asset at a relative path, as the scan would have produced it.
EditorAsset makeAsset(const std::string& relative) {
  const fs::path path(relative);
  return {path.stem().string(), fs::path("/project/assets") / path, path};
}

/// A tree over the given asset paths.
EditorAssetTree treeOf(const std::vector<std::string>& paths) {
  EditorAssetScan scan;
  for (const std::string& path : paths) {
    scan.assets.push_back(makeAsset(path));
  }
  return buildEditorAssetTree(scan);
}

/// Row for the folder named @p name, or nullptr.
const EditorAssetFolderRow*
rowFor(const std::vector<EditorAssetFolderRow>& rows,
       const EditorAssetTree& tree, const std::string& name) {
  for (const EditorAssetFolderRow& row : rows) {
    if (tree.folders[row.folder].name == name) {
      return &row;
    }
  }
  return nullptr;
}

}  // namespace

TEST_CASE("the root is always the first row") {
  const EditorAssetTree tree = treeOf({"crate.obj"});
  const auto rows = flattenAssetFolderRows(tree, {});

  REQUIRE(rows.size() == 1);
  REQUIRE(rows.front().folder == EDITOR_ASSET_FOLDER_ROOT);
  REQUIRE(rows.front().depth == 0);
}

TEST_CASE("a collapsed root hides the folders under it") {
  const EditorAssetTree tree = treeOf({"props/barrel.obj"});
  const auto rows = flattenAssetFolderRows(tree, {});

  REQUIRE(rows.size() == 1);
  REQUIRE(rows.front().has_children);
  REQUIRE_FALSE(rows.front().expanded);
}

TEST_CASE("expanding the root lists the folders under it") {
  const EditorAssetTree tree = treeOf({"props/barrel.obj", "terrain/mud.obj"});
  const auto rows = flattenAssetFolderRows(tree, {EDITOR_ASSET_FOLDER_ROOT});

  REQUIRE(rows.size() == 3);
  REQUIRE(rows.front().expanded);
  REQUIRE(rows[1].depth == 1);
  REQUIRE(rows[2].depth == 1);
}

TEST_CASE("a folder with no children cannot be expanded") {
  const EditorAssetTree tree = treeOf({"props/barrel.obj"});
  const auto rows = flattenAssetFolderRows(tree, {EDITOR_ASSET_FOLDER_ROOT});
  const EditorAssetFolderRow* props = rowFor(rows, tree, "props");

  REQUIRE(props != nullptr);
  REQUIRE_FALSE(props->has_children);
  REQUIRE_FALSE(props->expanded);
}

TEST_CASE("a nested folder is listed only when its parent is open") {
  const EditorAssetTree tree = treeOf({"terrain/rocks/boulder.obj"});
  const size_t terrain =
      tree.folders[EDITOR_ASSET_FOLDER_ROOT].child_folders.front();

  const auto closed = flattenAssetFolderRows(tree, {EDITOR_ASSET_FOLDER_ROOT});
  REQUIRE(rowFor(closed, tree, "rocks") == nullptr);

  const auto open =
      flattenAssetFolderRows(tree, {EDITOR_ASSET_FOLDER_ROOT, terrain});
  const EditorAssetFolderRow* rocks = rowFor(open, tree, "rocks");
  REQUIRE(rocks != nullptr);
  REQUIRE(rocks->depth == 2);
}

TEST_CASE("expanding a folder whose parent is shut lists nothing extra") {
  const EditorAssetTree tree = treeOf({"terrain/rocks/boulder.obj"});
  const size_t terrain =
      tree.folders[EDITOR_ASSET_FOLDER_ROOT].child_folders.front();

  // The root is shut, so nothing below it shows however it is marked.
  const auto rows = flattenAssetFolderRows(tree, {terrain});
  REQUIRE(rows.size() == 1);
}

TEST_CASE("rows come out in the tree's own order") {
  const EditorAssetTree tree =
      treeOf({"zed/a.obj", "alpha/b.obj", "mid/c.obj"});
  const auto rows = flattenAssetFolderRows(tree, {EDITOR_ASSET_FOLDER_ROOT});

  REQUIRE(rows.size() == 4);
  REQUIRE(tree.folders[rows[1].folder].name == "alpha");
  REQUIRE(tree.folders[rows[2].folder].name == "mid");
  REQUIRE(tree.folders[rows[3].folder].name == "zed");
}

TEST_CASE("an expanded folder that no longer exists is ignored") {
  const EditorAssetTree tree = treeOf({"crate.obj"});
  // What a rescan leaves behind: an index from the tree that came before.
  const auto rows = flattenAssetFolderRows(tree, {EDITOR_ASSET_FOLDER_ROOT, 7});

  REQUIRE(rows.size() == 1);
}

TEST_CASE("a tree with no root at all yields no rows") {
  EditorAssetTree tree;
  tree.folders.clear();

  REQUIRE(flattenAssetFolderRows(tree, {EDITOR_ASSET_FOLDER_ROOT}).empty());
}

TEST_CASE("a deep chain nests one level per folder") {
  const EditorAssetTree tree = treeOf({"a/b/c/leaf.obj"});
  std::unordered_set<size_t> expanded;
  for (size_t i = 0; i < tree.folders.size(); ++i) {
    expanded.insert(i);
  }

  const auto rows = flattenAssetFolderRows(tree, expanded);
  REQUIRE(rows.size() == 4);
  for (size_t i = 0; i < rows.size(); ++i) {
    REQUIRE(rows[i].depth == static_cast<uint32_t>(i));
  }
}
