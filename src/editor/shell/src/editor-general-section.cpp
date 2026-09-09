#include <editor/shell/editor-general-section.h>
#include <string>

namespace eng::editor {

namespace {

  /// Add a folder named @p name under @p parent, and return its index. The
  /// parent is told about it unless there is none, which is what makes a
  /// folder a section of its own.
  size_t addFolder(EditorAssetTree& tree, size_t parent,
                   std::string_view name) {
    const size_t index = tree.folders.size();
    EditorAssetFolder folder;
    folder.name = std::string(name);
    // No relative path: none of this stands for a directory on disk.
    folder.parent = parent;
    tree.folders.push_back(std::move(folder));
    if (parent != EDITOR_ASSET_FOLDER_NONE) {
      tree.folders[parent].child_folders.push_back(index);
    }
    return index;
  }

  /// Give the folder at @p index the @p count entries numbered from
  /// @p first.
  void fillFolder(EditorAssetTree& tree, size_t index, size_t first,
                  size_t count) {
    for (size_t entry = 0; entry < count; ++entry) {
      tree.folders[index].assets.push_back(first + entry);
    }
  }

}  // namespace

size_t appendEditorGeneralSection(EditorAssetTree& tree, size_t first_shape,
                                  size_t first_light) {
  const size_t section =
      addFolder(tree, EDITOR_ASSET_FOLDER_NONE, EDITOR_GENERAL_FOLDER_NAME);
  const size_t lighting = addFolder(tree, section, EDITOR_LIGHTING_FOLDER_NAME);
  fillFolder(tree, lighting, first_light, EDITOR_GENERAL_ITEM_COUNT);
  const size_t shapes = addFolder(tree, section, EDITOR_SHAPES_FOLDER_NAME);
  fillFolder(tree, shapes, first_shape, EDITOR_SHAPE_COUNT);
  // In front of the assets root: the pane lists the sections in this order,
  // which is the one decision about them the tree carries.
  tree.sections.insert(tree.sections.begin(), section);
  return section;
}

}  // namespace eng::editor
