#include <catch2/catch_test_macros.hpp>
#include <cstddef>
#include <editor/shell/editor-asset-folder-rows.h>
#include <editor/shell/editor-general-section.h>
#include <string>

using namespace eng::editor;

namespace {

/// A tree with the built-in section beside a root holding @p assets of
/// them, numbered as the editor numbers a browser's entries.
EditorAssetTree treeWithSection(size_t assets) {
  EditorAssetTree tree;
  for (size_t i = 0; i < assets; ++i) {
    tree.folders[EDITOR_ASSET_FOLDER_ROOT].assets.push_back(i);
  }
  appendEditorGeneralSection(tree, assets);
  return tree;
}

}  // namespace

TEST_CASE("the general section stands beside the assets root, not under it") {
  const EditorAssetTree tree = treeWithSection(0);
  const size_t section = tree.folders.size() - 1;

  REQUIRE(tree.folders[section].name == EDITOR_GENERAL_FOLDER_NAME);
  REQUIRE(tree.folders[section].parent == EDITOR_ASSET_FOLDER_NONE);
  REQUIRE(tree.folders[EDITOR_ASSET_FOLDER_ROOT].child_folders.empty());
}

TEST_CASE("the section holds every built-in item") {
  const EditorAssetTree tree = treeWithSection(0);
  const size_t section = tree.folders.size() - 1;

  REQUIRE(tree.folders[section].assets.size() == EDITOR_GENERAL_ITEM_COUNT);
}

TEST_CASE("built-in items are numbered after the assets") {
  const EditorAssetTree tree = treeWithSection(3);
  const size_t section = tree.folders.size() - 1;

  // The browser reports a drop by this number, and the editor tells an
  // asset from an item by where it falls — so the first item has to be the
  // one past the last asset.
  REQUIRE(tree.folders[section].assets.front() == 3);
  REQUIRE(tree.folders[section].assets.back() ==
          3 + EDITOR_GENERAL_ITEM_COUNT - 1);
}

TEST_CASE("the pane lists the section as the first top-level row") {
  const EditorAssetTree tree = treeWithSection(1);
  const auto rows = flattenAssetFolderRows(tree, {});

  REQUIRE(rows.size() == 2);
  REQUIRE(rows.front().folder == tree.folders.size() - 1);
  REQUIRE(rows.back().folder == EDITOR_ASSET_FOLDER_ROOT);
  // Beside the assets root rather than inside it, which is what "the same
  // level" means to whoever is looking at the pane.
  REQUIRE(rows.front().depth == 0);
  REQUIRE_FALSE(rows.front().has_children);
}

TEST_CASE("expanding the assets root does not push the section around") {
  EditorAssetTree tree;
  tree.folders.push_back({"props", "props", EDITOR_ASSET_FOLDER_ROOT});
  tree.folders[EDITOR_ASSET_FOLDER_ROOT].child_folders.push_back(1);
  appendEditorGeneralSection(tree, 0);
  const auto rows = flattenAssetFolderRows(tree, {EDITOR_ASSET_FOLDER_ROOT});

  // The section, then the assets root, then what the root holds — the two
  // sections keep their order however deep the tree under one of them goes.
  REQUIRE(rows.size() == 3);
  REQUIRE(tree.folders[rows[0].folder].name == EDITOR_GENERAL_FOLDER_NAME);
  REQUIRE(rows[0].depth == 0);
  REQUIRE(rows[1].folder == EDITOR_ASSET_FOLDER_ROOT);
  REQUIRE(rows[2].depth == 1);
}

TEST_CASE("a tree naming a section it does not have lists what it does") {
  EditorAssetTree tree;
  // An index left over from a larger tree names no folder here; walking it
  // would run off the end of the folder list.
  tree.sections.push_back(9);
  const auto rows = flattenAssetFolderRows(tree, {});

  REQUIRE(rows.size() == 1);
  REQUIRE(rows.front().folder == EDITOR_ASSET_FOLDER_ROOT);
}
