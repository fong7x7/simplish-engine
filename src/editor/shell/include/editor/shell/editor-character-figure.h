#pragma once

/// @file editor-character-figure.h
/// @brief One character the viewport draws: a player in a playtest, or the
/// one standing on a player start.
/// @par Threading Main-thread-only.

#include <editor/shell/editor-character-gait.h>
#include <editor/shell/editor-document.h>
#include <engine/math/vec2.h>
#include <engine/math/vec3.h>
#include <string>
#include <vector>

namespace eng::editor {

/// A character to draw this frame, and everything the draw needs to know
/// about it that is not in the asset.
/// @thread_safety Main-thread-only.
struct EditorCharacterFigure {
  /// What keeps its animation from frame to frame: `player_start:start_01`
  /// for a start's, `player:1` for a playtest's. Never a placement's id,
  /// which has no colon, so the two share one animator without meeting.
  std::string key{};
  /// The asset reference it is drawn as, or empty for the stand-in.
  std::string character{};
  /// Where its feet are, in tiles.
  Vec3 feet{};
  /// The direction it faces, world X and Y; need not be unit length.
  Vec2 aim{1.0f, 0.0f};
  /// Whether it is walking, which picks its clip.
  EditorCharacterGait gait = EditorCharacterGait::STILL;
};

/// The characters standing on @p document's player starts: one per start
/// that has a character, facing +X — the way a player spawns aiming —
/// standing still. A start with none has only its marker, as before.
[[nodiscard]] std::vector<EditorCharacterFigure>
editorStartFigures(const EditorDocument& document);

}  // namespace eng::editor
