#pragma once

/// @file editor-frame-event.h
/// @brief A sound a sprite sheet plays when it reaches a frame.
/// @par Threading A value type.

#include <cstdint>
#include <string>

namespace eng::editor {

/// A frame of a sprite sheet, and the sound it makes when a billboard
/// showing the sheet reaches it — the 2D counterpart of
/// `EditorAnimationEvent`, by frame rather than by second because a sheet's
/// speed is each billboard's own.
struct EditorFrameEvent {
  /// Which frame, from 0, counted left to right and then down.
  uint16_t frame = 0;
  /// What it plays, as `EditorAnimationEvent::sound`.
  std::string sound{};
  /// How loud, where 1 is as recorded.
  float gain = 1.0F;

  /// Two events are the same when all three agree.
  bool operator==(const EditorFrameEvent&) const = default;
};

}  // namespace eng::editor
