#pragma once

/// @file editor-placement-animator.h
/// @brief The playback of every rigged prop in the viewport, kept from frame
/// to frame so a change of clip fades rather than snaps.
/// @par Threading Main-thread only.

#include <cstddef>
#include <editor/shell/editor-placement.h>
#include <engine/animation/clip-player.h>
#include <engine/animation/rig.h>
#include <engine/math/mat4.h>
#include <map>
#include <span>
#include <string>

namespace eng::editor {

/// One `ClipPlayer` per rigged placement, found by the placement's id.
///
/// By id rather than by position in the placement list, because that list
/// is renumbered by every delete and undo, and a player found by position
/// would hand one prop's fade to another. Players live in a `std::map`
/// because its entries never move: the skin span `pose` returns points into
/// a player, and it has to survive the players added after it in the same
/// frame.
/// @thread_safety Main-thread only.
class EditorPlacementAnimator {
public:
  /// An animator whose clip changes fade over @p fade_seconds.
  explicit EditorPlacementAnimator(
      float fade_seconds = animation::CLIP_DEFAULT_FADE_SECONDS)
    : fade_seconds_(fade_seconds) {}

  /// Skin matrices for @p placement at clock time @p now, valid until the
  /// placement is next posed or forgotten. When the clip the placement
  /// names has changed since the last frame, its player starts fading to
  /// it; a placement seen for the first time cuts straight in, since there
  /// is nothing on screen yet to fade from.
  std::span<const Mat4> pose(const EditorPlacement& placement,
                             const animation::Rig& rig, double now);

  /// Forget every placement not posed since the last call: the ones deleted,
  /// or whose model has gone. Called once a frame, after the posing.
  void endFrame();

  /// How many placements have a player.
  [[nodiscard]] size_t size() const { return players_.size(); }

private:
  /// One placement's playback.
  struct Entry {
    /// Its clips, and the fade between them.
    animation::ClipPlayer player;
    /// Whether it was posed this frame, which is what keeps it past
    /// `endFrame`.
    bool posed = false;
  };

  /// Every rigged placement's playback, by placement id.
  std::map<std::string, Entry> players_{};
  /// Seconds a change of clip fades over.
  float fade_seconds_ = animation::CLIP_DEFAULT_FADE_SECONDS;
};

}  // namespace eng::editor
