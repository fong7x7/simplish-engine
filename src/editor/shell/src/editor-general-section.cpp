#include <array>
#include <cstddef>
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

  /// One subsection of the general section: what it is called, and the run
  /// of entry numbers it holds.
  struct Subsection {
    /// Its name in the folder pane.
    std::string_view name;
    /// The first entry number it holds.
    size_t first;
    /// How many it holds.
    size_t count;
  };

  /// How many subsections there are.
  constexpr size_t SUBSECTION_COUNT = 5;

  /// The subsections, in the order the pane lists them, each holding a run
  /// of the built-in items numbered from @p first_item — except the
  /// shapes, which are assets and sit in the asset list's own numbering
  /// from @p first_shape.
  std::array<Subsection, SUBSECTION_COUNT> subsectionsOf(size_t first_shape,
                                                         size_t first_item) {
    const size_t tools = first_item + EDITOR_GENERAL_LIGHT_COUNT;
    const size_t effects = tools + EDITOR_GENERAL_TOOL_COUNT;
    const size_t sprites = effects + EDITOR_GENERAL_EFFECT_COUNT;
    return {
        {{EDITOR_LIGHTING_FOLDER_NAME, first_item, EDITOR_GENERAL_LIGHT_COUNT},
         {EDITOR_SHAPES_FOLDER_NAME, first_shape, EDITOR_SHAPE_COUNT},
         {EDITOR_TOOLS_FOLDER_NAME, tools, EDITOR_GENERAL_TOOL_COUNT},
         {EDITOR_EFFECTS_FOLDER_NAME, effects, EDITOR_GENERAL_EFFECT_COUNT},
         {EDITOR_SPRITES_FOLDER_NAME, sprites, EDITOR_GENERAL_SPRITE_COUNT}}};
  }

}  // namespace

size_t appendEditorGeneralSection(EditorAssetTree& tree, size_t first_shape,
                                  size_t first_item) {
  const size_t section =
      addFolder(tree, EDITOR_ASSET_FOLDER_NONE, EDITOR_GENERAL_FOLDER_NAME);
  for (const Subsection& sub : subsectionsOf(first_shape, first_item)) {
    fillFolder(tree, addFolder(tree, section, sub.name), sub.first, sub.count);
  }
  // In front of the assets root: the pane lists the sections in this order,
  // which is the one decision about them the tree carries.
  tree.sections.insert(tree.sections.begin(), section);
  return section;
}

}  // namespace eng::editor
