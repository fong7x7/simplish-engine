#pragma once

/// @file editor-clip-pass.h
/// @brief The stretch of a clip one rigged thing played in a frame.
/// @par Threading A value type.

#include <cstddef>
#include <engine/animation/clip-window.h>
#include <engine/math/vec3.h>
#include <string>

namespace eng::editor {

/// What one frame's posing did with one rigged thing's clip: which it was,
/// how far through it playback moved, and where the thing stood — all the
/// clip's events need to be heard from.
struct EditorClipPass {
  /// Whose it was: a placement's id, or a character figure's key
  /// (`player:1`).
  std::string key{};
  /// Where its feet were, in world tiles.
  Vec3 at{};
  /// Its model, in the editor's asset list.
  size_t asset = 0;
  /// The clip, in the model's rig.
  size_t clip = 0;
  /// How far through the clip playback moved this frame.
  animation::ClipWindow window{};
};

}  // namespace eng::editor
