#include <algorithm>
#include <editor/shell/editor-placement-clip.h>
#include <engine/animation/rig-pose.h>

namespace eng::editor {

std::vector<std::string> editorClipNames(const animation::Rig* rig) {
  std::vector<std::string> names;
  if (rig == nullptr) {
    return names;
  }
  for (const animation::AnimationClip& clip : rig->clips) {
    names.push_back(clip.name);
  }
  return names;
}

size_t editorClipIndex(std::span<const std::string> names,
                       const std::string& name) {
  const auto found = std::ranges::find(names, name);
  return found == names.end() ? 0 : static_cast<size_t>(found - names.begin());
}

size_t editorPlacementClip(const EditorPlacement& placement,
                           const animation::Rig* rig) {
  if (rig == nullptr || rig->clips.empty()) {
    return animation::RIG_REST_POSE;
  }
  const auto found = std::ranges::find(rig->clips, placement.animation,
                                       &animation::AnimationClip::name);
  return found == rig->clips.end()
             ? 0
             : static_cast<size_t>(found - rig->clips.begin());
}

std::string stepEditorClip(std::span<const std::string> names,
                           const std::string& current, int steps) {
  if (names.empty()) {
    return {};
  }
  const auto count = static_cast<long>(names.size());
  const auto from = static_cast<long>(editorClipIndex(names, current));
  // Wrapped into [0, count) whichever way it stepped and however far.
  const long to = ((from + steps) % count + count) % count;
  return names[static_cast<size_t>(to)];
}

}  // namespace eng::editor
