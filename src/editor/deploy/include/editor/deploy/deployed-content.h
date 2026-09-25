#pragma once

/// @file deployed-content.h
/// @brief Reading the content a deploy — or a logic check — baked.
/// @par Threading Main-thread-only (reads the disk).

#include <filesystem>
#include <game/content/game-content.h>
#include <game/world/game-setup.h>
#include <optional>
#include <string>

namespace eng::editor {

/// The setup baked for @p level in the content at @p content, or nothing
/// when there is none.
[[nodiscard]] std::optional<game::GameSetup>
readDeployedSetup(const std::filesystem::path& content,
                  const std::string& level);

/// The data tables beside the content at @p content: the project's, read
/// as the editor reads them.
[[nodiscard]] game::GameContent
readDeployedContent(const std::filesystem::path& content);

}  // namespace eng::editor
