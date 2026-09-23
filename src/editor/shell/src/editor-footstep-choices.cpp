#include <editor/shell/editor-footstep-choices.h>
#include <game/content/footstep-names.h>

namespace eng::editor {

namespace {

  /// The Surface row's first choice.
  constexpr std::string_view FROM_THE_GROUND = "From the ground";

}  // namespace

std::vector<std::string> editorSurfaceChoiceNames() {
  std::vector<std::string> names{std::string(FROM_THE_GROUND)};
  for (const game::FootstepSurface surface : game::ALL_FOOTSTEP_SURFACES) {
    names.emplace_back(game::footstepSurfaceLabel(surface));
  }
  return names;
}

size_t editorSurfaceChoiceIndex(std::optional<game::FootstepSurface> surface) {
  return surface ? static_cast<size_t>(*surface) + 1 : 0;
}

std::optional<game::FootstepSurface> editorSurfaceChoice(size_t index) {
  if (index == 0 || index > game::FOOTSTEP_SURFACE_COUNT) {
    return std::nullopt;
  }
  return game::ALL_FOOTSTEP_SURFACES[index - 1];
}

std::vector<std::string> editorFootstepChoiceNames() {
  std::vector<std::string> names;
  names.reserve(game::STEP_SET_COUNT);
  for (const game::StepSet steps : game::ALL_STEP_SETS) {
    names.emplace_back(game::stepSetLabel(steps));
  }
  return names;
}

}  // namespace eng::editor
