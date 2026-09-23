#include <algorithm>
#include <editor/shell/editor-placement-animator.h>
#include <editor/shell/editor-placement-clip.h>
#include <iterator>
#include <utility>

namespace eng::editor {

std::span<const Mat4>
EditorPlacementAnimator::pose(const EditorPlacement& placement,
                              const animation::Rig& rig, double now) {
  Entry& entry = players_[placement.id];
  entry.posed = true;
  entry.player.play(editorPlacementClip(placement, &rig), now, fade_seconds_);
  notePass(entry, placement, now);
  return entry.player.evaluate(rig, now);
}

void EditorPlacementAnimator::notePass(Entry& entry,
                                       const EditorPlacement& placement,
                                       double now) {
  // Just before its start, for a clip just begun, so an event at zero is
  // in the first frame's pass rather than a moment no frame covers.
  constexpr double BEFORE_START = -1e-6;
  const size_t clip = entry.player.clip();
  const double seconds = now - entry.player.clipStarted();
  const double from = clip == entry.last_clip ? entry.last_seconds
                                              : std::min(BEFORE_START, seconds);
  entry.last_clip = clip;
  entry.last_seconds = seconds;
  if (clip != animation::RIG_REST_POSE) {
    const WorldPoint& at = placement.position;
    passes_.push_back({placement.id,
                       {at.x, at.y, at.z},
                       placement.asset,
                       clip,
                       {from, seconds}});
  }
}

std::vector<EditorClipPass> EditorPlacementAnimator::takePasses() {
  return std::exchange(passes_, {});
}

void EditorPlacementAnimator::endFrame() {
  for (auto it = players_.begin(); it != players_.end();) {
    it = it->second.posed ? std::next(it) : players_.erase(it);
  }
  for (auto& [id, entry] : players_) {
    static_cast<void>(id);
    entry.posed = false;
  }
}

}  // namespace eng::editor
