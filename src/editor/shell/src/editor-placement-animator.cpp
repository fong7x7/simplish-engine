#include <editor/shell/editor-placement-animator.h>
#include <editor/shell/editor-placement-clip.h>
#include <iterator>

namespace eng::editor {

std::span<const Mat4>
EditorPlacementAnimator::pose(const EditorPlacement& placement,
                              const animation::Rig& rig, double now) {
  Entry& entry = players_[placement.id];
  entry.posed = true;
  entry.player.play(editorPlacementClip(placement, &rig), now, fade_seconds_);
  return entry.player.evaluate(rig, now);
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
