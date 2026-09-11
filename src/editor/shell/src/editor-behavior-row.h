#pragma once

/// @file editor-behavior-row.h
/// @brief Reading one row of the behaviors table.
/// @par Threading Thread-safe (pure functions).

#include <game/content/behavior-definition.h>
#include <nlohmann/json.hpp>
#include <optional>
#include <string>
#include <vector>

namespace eng::editor {

/// The behavior row @p entry defines, its `id` already checked usable, or
/// nothing — said why in @p problems — when it has no state worth keeping.
/// Every other problem with the row is said in @p problems too.
[[nodiscard]] std::optional<game::BehaviorDefinition>
readEditorBehaviorRow(const nlohmann::json& entry, const std::string& id,
                      std::vector<std::string>& problems);

}  // namespace eng::editor
