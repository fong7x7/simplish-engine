#pragma once

/// @file editor-general-section.h
/// @brief The browser's built-in section, beside a project's asset folders.
/// @par Threading Thread-safe (pure function over value types).

#include <cstddef>
#include <editor/shell/editor-asset-tree.h>
#include <editor/shell/editor-general-item.h>
#include <string_view>

namespace eng::editor {

/// What the section is called in the folder pane. Lowercase, as the assets
/// root is: the two are the pane's two sections and should read alike.
inline constexpr std::string_view EDITOR_GENERAL_FOLDER_NAME = "general";

/// Add the general section to @p tree as a top-level folder above the
/// assets root, holding every built-in item, and return its index.
///
/// A sibling of the assets root rather than a folder inside it: nothing in
/// it comes from the project's assets directory, and listing it under that
/// directory's name would say it did. It goes above rather than below
/// because it is the same short list in every project, while the assets
/// root is a tree that grows — a fixed row is easier to reach at the top
/// than after however many folders a project has.
///
/// @p first_entry is the number the section's first item takes in the
/// browser's entry numbering — the count of scanned assets, since the
/// built-in items are numbered after them. That numbering is what a folder
/// holds and what a drop reports, so the caller has to name the assets and
/// the items in the same order it passes here.
size_t appendEditorGeneralSection(EditorAssetTree& tree, size_t first_entry);

}  // namespace eng::editor
