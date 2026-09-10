#pragma once

/// @file editor-general-section.h
/// @brief The browser's built-in section, beside a project's asset folders.
/// @par Threading Thread-safe (pure function over value types).

#include <cstddef>
#include <editor/shell/editor-asset-tree.h>
#include <editor/shell/editor-general-item.h>
#include <editor/shell/editor-shape-kind.h>
#include <string_view>

namespace eng::editor {

/// What the section is called in the folder pane. Lowercase, as the assets
/// root is: the two are the pane's two sections and should read alike.
inline constexpr std::string_view EDITOR_GENERAL_FOLDER_NAME = "general";

/// The subsection holding the light sources.
inline constexpr std::string_view EDITOR_LIGHTING_FOLDER_NAME = "lighting";

/// The subsection holding the built-in shapes.
inline constexpr std::string_view EDITOR_SHAPES_FOLDER_NAME = "shapes";

/// The subsection holding the tools: things that mark the level for the
/// game — where a player starts — rather than things that show in it.
inline constexpr std::string_view EDITOR_TOOLS_FOLDER_NAME = "tools";

/// Add the general section to @p tree as a top-level folder above the
/// assets root, holding a folder of lights, a folder of shapes and a folder
/// of tools, and return the section's index.
///
/// A sibling of the assets root rather than a folder inside it: nothing in
/// it comes from the project's assets directory, and listing it under that
/// directory's name would say it did. It goes above rather than below
/// because it is the same short list in every project, while the assets
/// root is a tree that grows — a fixed row is easier to reach at the top
/// than after however many folders a project has.
///
/// The section holds nothing itself. Three kinds of built-in thing are too
/// many for a single grid of cards to read as anything but a pile, and the
/// subsections are what a designer reaching for a light rather than a box
/// actually navigates by.
///
/// Both numbers are entry numbers in the browser's own numbering, which is
/// what a folder holds and what a drop reports: @p first_shape is where the
/// built-in shapes sit in the editor's asset list, and @p first_item is the
/// first number past every asset, where `EDITOR_GENERAL_ITEMS` are counted
/// in order. The caller has to name its entries in that order.
size_t appendEditorGeneralSection(EditorAssetTree& tree, size_t first_shape,
                                  size_t first_item);

}  // namespace eng::editor
