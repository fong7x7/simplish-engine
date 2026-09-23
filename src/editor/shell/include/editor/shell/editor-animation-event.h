#pragma once

/// @file editor-animation-event.h
/// @brief A sound a clip plays at a moment of it.
/// @par Threading A value type.

#include <string>
#include <string_view>

namespace eng::editor {

/// What an event plays that is not a sound of its own: a footstep, picked
/// by the feet of whoever is animating and the surface under them, as a
/// stride's step is (audio.md §9.1).
inline constexpr std::string_view EDITOR_FOOTSTEP_EVENT = "footstep";

/// A moment of an animation clip, and the sound it makes.
struct EditorAnimationEvent {
  /// Seconds into the clip.
  float at = 0.0F;
  /// What it plays: `footstep`; one of the game's sound slots
  /// (`combat.blast`, `step.boots.wood`); or a `.wav` or `.ogg` under the
  /// project's assets, by its path there (`sounds/swoosh.wav`).
  std::string sound{};
  /// How loud, where 1 is as recorded.
  float gain = 1.0F;

  /// Two events are the same when all three agree.
  bool operator==(const EditorAnimationEvent&) const = default;
};

}  // namespace eng::editor
