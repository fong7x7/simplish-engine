#pragma once

/// @file editor-widget-query.h
/// @brief Which of the editor's widgets `get_widgets` describes.
/// @par Threading
/// A value type.

#include <cstddef>
#include <string>

namespace eng::editor {

/// The part of the editor's widget tree an agent asked about.
struct EditorWidgetQuery {
  /// The widget to start from, by its `id` or its debug name; empty for
  /// the whole tree.
  std::string under{};
  /// How many levels below it to describe; deeper widgets are counted,
  /// not listed.
  size_t depth = 6;
  /// Whether hidden widgets — and what is under them — are listed too.
  bool hidden = false;
};

}  // namespace eng::editor
