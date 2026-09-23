#include "editor-step-set-read.h"

#include <game/content/footstep-names.h>

namespace eng::editor {

game::StepSet readEditorStepSet(const nlohmann::json& entry,
                                const std::string& id,
                                std::vector<std::string>& problems) {
  const auto found = entry.find("footsteps");
  if (found == entry.end()) {
    return game::StepSet::DEFAULT;
  }
  const std::optional<game::StepSet> steps =
      found->is_string() ? game::stepSetNamed(found->get<std::string>())
                         : std::nullopt;
  if (!steps) {
    problems.push_back(id + ": footsteps is not default, boots, bare, claws "
                            "or heavy, so it is default");
  }
  return steps.value_or(game::StepSet::DEFAULT);
}

}  // namespace eng::editor
