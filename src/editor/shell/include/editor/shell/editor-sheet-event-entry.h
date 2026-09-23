#pragma once

/// @file editor-sheet-event-entry.h
/// @brief The events a project gives one sprite sheet.
/// @par Threading A value type.

#include <editor/shell/editor-frame-event.h>
#include <string>
#include <vector>

namespace eng::editor {

/// One row of the animation events table for a sprite sheet: what each of
/// its frames sounds like as a billboard plays it.
struct EditorSheetEventEntry {
  /// The sheet, by its path under the project's assets:
  /// `sprites/slime.png`, as a billboard names it.
  std::string sheet{};
  /// Its events, in frame order.
  std::vector<EditorFrameEvent> events{};

  /// Two rows are the same when both agree.
  bool operator==(const EditorSheetEventEntry&) const = default;
};

}  // namespace eng::editor
