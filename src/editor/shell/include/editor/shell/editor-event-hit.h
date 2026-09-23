#pragma once

/// @file editor-event-hit.h
/// @brief One animation event a frame's playback reached.
/// @par Threading A value type.

#include <engine/math/vec3.h>
#include <string>

namespace eng::editor {

/// An event reached this frame, and whose and where it was.
struct EditorEventHit {
  /// Whose animation it was: a placement's id, a character figure's key
  /// (`player:1`), or a billboard's id.
  std::string key{};
  /// Where it happened, in world tiles.
  Vec3 at{};
  /// What it plays, as `EditorAnimationEvent::sound`.
  std::string sound{};
  /// How loud.
  float gain = 1.0F;
};

}  // namespace eng::editor
