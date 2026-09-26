#pragma once

/// @file deployed-content.h
/// @brief Reading the content a deploy — or a logic check — baked.
/// @par Threading Main-thread-only (reads the disk).

#include <cstdint>
#include <filesystem>
#include <game/content/game-content.h>
#include <game/logic/game-logic-instance.h>
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

/// A hash of the files in the content at @p content that the simulation
/// reads — the manifest, `levels/` and `content/`: paths and bytes, in
/// path order. Two deployed games agree on it only when they would
/// simulate the same run, which is what a server checks a joining client
/// against (ADR-013).
[[nodiscard]] uint64_t
deployedContentHash(const std::filesystem::path& content);

/// The deployed game's name, as its manifest gives it; the folder's name
/// when it gives none.
[[nodiscard]] std::string
deployedGameName(const std::filesystem::path& content);

/// Give @p setup room for @p logic to spawn into, as a playtest does, when
/// there is logic.
void makeRoomForLogic(game::GameSetup& setup,
                      const game::GameLogicInstance& logic);

}  // namespace eng::editor
