#pragma once

/// @file editor-setup-json.h
/// @brief A run's setup, written out for a deployed game to start from.
/// @par Threading Thread-safe (pure functions).

#include <game/world/game-setup.h>
#include <optional>
#include <string>
#include <string_view>

namespace eng::editor {

/// Schema tag of a baked setup file.
inline constexpr std::string_view EDITOR_SETUP_SCHEMA = "simplish/setup/1.0";

/// @p setup as JSON: every field, floats written so they read back to the
/// same bits — the setup is simulation input, and a deployed game has to
/// start the run the playtest started.
[[nodiscard]] std::string serializeGameSetup(const game::GameSetup& setup);

/// A setup read back from @p text, or nothing when it is not one.
[[nodiscard]] std::optional<game::GameSetup>
parseGameSetup(std::string_view text);

}  // namespace eng::editor
