#pragma once

/// @file editor-clip-event-set.h
/// @brief The events one clip plays, and where they came from.
/// @par Threading A value type.

#include <editor/shell/editor-animation-event.h>
#include <editor/shell/editor-event-source.h>
#include <vector>

namespace eng::editor {

/// What one clip of a rigged model plays as it runs.
struct EditorClipEventSet {
  /// Its events, in time order.
  std::vector<EditorAnimationEvent> events{};
  /// Where they came from.
  EditorEventSource source = EditorEventSource::NONE;
};

}  // namespace eng::editor
