#include <algorithm>
#include <cctype>
#include <editor/shell/editor-placement-clip.h>
#include <engine/animation/rig-pose.h>
#include <string_view>

namespace eng::editor {

namespace {

  /// Whether @p name has @p word in it, ignoring case.
  bool mentions(const std::string& name, std::string_view word) {
    const auto lower = [](char c) {
      return static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    };
    return !std::ranges::search(name, word, {}, lower, lower).empty();
  }

  /// The first of @p names with @p word in it, or empty.
  std::string firstNamed(std::span<const std::string> names,
                         std::string_view word) {
    const auto found =
        std::ranges::find_if(names, [word](const std::string& name) {
          return mentions(name, word);
        });
    return found == names.end() ? std::string{} : *found;
  }

}  // namespace

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

std::string editorCharacterClip(std::span<const std::string> names,
                                EditorCharacterGait gait) {
  if (gait == EditorCharacterGait::MOVING) {
    const std::string running = firstNamed(names, "run");
    return running.empty() ? firstNamed(names, "walk") : running;
  }
  return firstNamed(names, "idle");
}

}  // namespace eng::editor
