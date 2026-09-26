#pragma once

/// @file editor-widgets-json.h
/// @brief The editor's own widget tree, as `get_widgets` answers with it.
/// @par Threading
/// Main-thread-only, with the tree it reads.

#include <editor/shell/editor-widget-query.h>
#include <engine/gui/gui-widget-tree.h>
#include <string>

namespace eng::editor {

/// @p tree as @p query asks for it, as JSON: `widgets`, the widget asked
/// for and its children, each with its type, `id`, `name` (its debug
/// name), `rect` `[x, y, w, h]` in layout pixels, its flags and its
/// `children` — or `more`, how many were left out past the depth;
/// `count`, how many are listed; and `error` when the widget asked for is
/// not there.
[[nodiscard]] std::string editorWidgetsJson(const GuiWidgetTree& tree,
                                            const EditorWidgetQuery& query);

}  // namespace eng::editor
