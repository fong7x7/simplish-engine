#pragma once

/// @file editor-clip-event-entry.h
/// @brief The events a project gives one clip of one model.
/// @par Threading A value type.

#include <editor/shell/editor-animation-event.h>
#include <string>
#include <vector>

namespace eng::editor {

/// One row of the animation events table for a rigged model's clip. Its
/// events replace the foot contacts found in the clip — an empty list
/// silences a clip the detection hears wrongly.
struct EditorClipEventEntry {
  /// The model, by asset reference: `mesh:characters_knight`.
  std::string asset{};
  /// The clip, by the name its file gives it: `walk`.
  std::string clip{};
  /// Its events, in time order.
  std::vector<EditorAnimationEvent> events{};

  /// Two rows are the same when all three agree.
  bool operator==(const EditorClipEventEntry&) const = default;
};

}  // namespace eng::editor
