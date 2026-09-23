#pragma once

/// @file editor-step-set-read.h
/// @brief A data table row's `footsteps`, shared by the characters and the
/// enemies tables.
/// @par Threading Thread-safe (pure function, beyond the problems list).

#include <game/content/step-set.h>
#include <nlohmann/json.hpp>
#include <string>
#include <vector>

namespace eng::editor {

/// What row @p entry, whose id is @p id, says its feet sound like: the
/// default when it says nothing, and — noted in @p problems — when it
/// names a step set there is none of.
[[nodiscard]] game::StepSet
readEditorStepSet(const nlohmann::json& entry, const std::string& id,
                  std::vector<std::string>& problems);

}  // namespace eng::editor
