#include <catch2/catch_test_macros.hpp>
#include <cstddef>
#include <editor/shell/editor-asset-folder-rows.h>
#include <editor/shell/editor-general-section.h>
#include <string>

using namespace eng::editor;

namespace {

/// A tree with the built-in section beside a root holding @p assets of
/// them, numbered as the editor numbers a browser's entries: the scanned
/// assets, then the built-in shapes, then the general items.
EditorAssetTree treeWithSection(size_t assets) {
  EditorAssetTree tree;
  for (size_t i = 0; i < assets; ++i) {
    tree.folders[EDITOR_ASSET_FOLDER_ROOT].assets.push_back(i);
  }
  appendEditorGeneralSection(tree, assets, assets + EDITOR_SHAPE_COUNT);
  return tree;
}

/// The folder named @p name, or `EDITOR_ASSET_FOLDER_NONE`.
size_t folderNamed(const EditorAssetTree& tree, std::string_view name) {
  for (size_t i = 0; i < tree.folders.size(); ++i) {
    if (tree.folders[i].name == name) {
      return i;
    }
  }
  return EDITOR_ASSET_FOLDER_NONE;
}

}  // namespace

TEST_CASE("the general section stands beside the assets root, not under it") {
  const EditorAssetTree tree = treeWithSection(0);
  const size_t section = folderNamed(tree, EDITOR_GENERAL_FOLDER_NAME);

  REQUIRE(section != EDITOR_ASSET_FOLDER_NONE);
  REQUIRE(tree.folders[section].parent == EDITOR_ASSET_FOLDER_NONE);
  REQUIRE(tree.folders[EDITOR_ASSET_FOLDER_ROOT].child_folders.empty());
}

TEST_CASE("the section holds three subsections and nothing itself") {
  const EditorAssetTree tree = treeWithSection(0);
  const size_t section = folderNamed(tree, EDITOR_GENERAL_FOLDER_NAME);

  // Three kinds of built-in thing in one grid of cards would read as a
  // pile; the subsections are what somebody reaching for a light navigates
  // by.
  REQUIRE(tree.folders[section].assets.empty());
  REQUIRE(tree.folders[section].child_folders.size() == 3);
}

TEST_CASE("the lighting subsection holds every light") {
  const EditorAssetTree tree = treeWithSection(3);
  const size_t lighting = folderNamed(tree, EDITOR_LIGHTING_FOLDER_NAME);

  REQUIRE(tree.folders[lighting].parent ==
          folderNamed(tree, EDITOR_GENERAL_FOLDER_NAME));
  REQUIRE(tree.folders[lighting].assets.size() == EDITOR_GENERAL_LIGHT_COUNT);
  // Past the three assets and the shapes among them: a light is not an
  // asset, so it is numbered after every one of them.
  REQUIRE(tree.folders[lighting].assets.front() == 3 + EDITOR_SHAPE_COUNT);
}

TEST_CASE("the tools subsection holds the player start, after the lights") {
  const EditorAssetTree tree = treeWithSection(3);
  const size_t tools = folderNamed(tree, EDITOR_TOOLS_FOLDER_NAME);

  REQUIRE(tools != EDITOR_ASSET_FOLDER_NONE);
  REQUIRE(tree.folders[tools].parent ==
          folderNamed(tree, EDITOR_GENERAL_FOLDER_NAME));
  REQUIRE(tree.folders[tools].assets.size() == EDITOR_GENERAL_TOOL_COUNT);
  // Numbered as the general items are listed, so the entry a drop reports
  // is the index of the item it names past the assets.
  const size_t first_item = 3 + EDITOR_SHAPE_COUNT;
  const size_t entry = tree.folders[tools].assets.front();
  REQUIRE(entry - first_item < EDITOR_GENERAL_ITEM_COUNT);
  REQUIRE(EDITOR_GENERAL_ITEMS[entry - first_item] ==
          EditorGeneralItem::PLAYER_START);
}

TEST_CASE("only the light items name a light kind") {
  REQUIRE(editorGeneralItemLightKind(EditorGeneralItem::POINT_LIGHT) ==
          EditorLightKind::POINT);
  REQUIRE_FALSE(
      editorGeneralItemLightKind(EditorGeneralItem::PLAYER_START).has_value());
  REQUIRE(editorGeneralItemName(EditorGeneralItem::PLAYER_START) ==
          "Player Start");
}

TEST_CASE("the shapes subsection holds every built-in shape") {
  const EditorAssetTree tree = treeWithSection(3);
  const size_t shapes = folderNamed(tree, EDITOR_SHAPES_FOLDER_NAME);

  REQUIRE(tree.folders[shapes].parent ==
          folderNamed(tree, EDITOR_GENERAL_FOLDER_NAME));
  REQUIRE(tree.folders[shapes].assets.size() == EDITOR_SHAPE_COUNT);
  // The shapes are assets, sitting on the end of the asset list, so their
  // entry numbers are the ones straight after the scanned three.
  REQUIRE(tree.folders[shapes].assets.front() == 3);
  REQUIRE(tree.folders[shapes].assets.back() == 3 + EDITOR_SHAPE_COUNT - 1);
}

TEST_CASE("the pane lists the section as the first top-level row") {
  const EditorAssetTree tree = treeWithSection(1);
  const auto rows = flattenAssetFolderRows(tree, {});

  REQUIRE(rows.size() == 2);
  REQUIRE(rows.front().folder == folderNamed(tree, EDITOR_GENERAL_FOLDER_NAME));
  REQUIRE(rows.back().folder == EDITOR_ASSET_FOLDER_ROOT);
  // Beside the assets root rather than inside it, which is what "the same
  // level" means to whoever is looking at the pane.
  REQUIRE(rows.front().depth == 0);
  REQUIRE(rows.front().has_children);
}

TEST_CASE("an open section lists its subsections under it") {
  const EditorAssetTree tree = treeWithSection(1);
  const size_t section = folderNamed(tree, EDITOR_GENERAL_FOLDER_NAME);
  const auto rows = flattenAssetFolderRows(tree, {section});

  REQUIRE(rows.size() == 5);
  REQUIRE(tree.folders[rows[1].folder].name == EDITOR_LIGHTING_FOLDER_NAME);
  REQUIRE(rows[1].depth == 1);
  REQUIRE(tree.folders[rows[2].folder].name == EDITOR_SHAPES_FOLDER_NAME);
  REQUIRE(rows[2].depth == 1);
  REQUIRE(tree.folders[rows[3].folder].name == EDITOR_TOOLS_FOLDER_NAME);
  REQUIRE(rows[3].depth == 1);
  // The assets root keeps its place at the bottom, whatever the section
  // above it is showing.
  REQUIRE(rows[4].folder == EDITOR_ASSET_FOLDER_ROOT);
  REQUIRE(rows[4].depth == 0);
}

TEST_CASE("expanding the assets root does not push the section around") {
  EditorAssetTree tree;
  tree.folders.push_back({"props", "props", EDITOR_ASSET_FOLDER_ROOT});
  tree.folders[EDITOR_ASSET_FOLDER_ROOT].child_folders.push_back(1);
  appendEditorGeneralSection(tree, 0, EDITOR_SHAPE_COUNT);
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
