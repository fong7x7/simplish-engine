#pragma once

/// @file editor-build-paths.h
/// @brief Where each thing the editor builds for a project goes.
/// @par Threading Thread-safe (pure functions over paths).

#include <cstdint>
#include <editor/build/editor-build-kind.h>
#include <filesystem>
#include <string_view>

namespace eng::editor {

/// The file a project's game logic library is written to, before its
/// platform's extension.
inline constexpr std::string_view LOGIC_LIBRARY_STEM = "game-logic";

/// The deployed game's executable, before its platform's extension.
inline constexpr std::string_view DEPLOYED_GAME_STEM = "simplish-game";

/// The folder, inside a deployed game, holding the project's content.
inline constexpr std::string_view DEPLOYED_CONTENT_DIR_NAME = "game";

/// Where the logic library is built (`<root>/build/logic`).
[[nodiscard]] std::filesystem::path
projectLogicBuildPath(const std::filesystem::path& root);

/// The logic library the build makes
/// (`<root>/build/logic/game-logic.dylib`, `.so` or `.dll`).
[[nodiscard]] std::filesystem::path
projectLogicLibraryPath(const std::filesystem::path& root);

/// Where the editor copies the library before it loads it, the
/// @p generation-th load of the session
/// (`<root>/build/logic/loaded/game-logic-3.dylib`). A copy, so the build
/// can write the next library while this one is loaded, and so each load
/// is a file the platform has never loaded before rather than one it
/// hands back from its cache.
[[nodiscard]] std::filesystem::path
projectLogicLoadPath(const std::filesystem::path& root, uint32_t generation);

/// Where the engine is configured and built for a deploy
/// (`<root>/build/deploy-cmake`).
[[nodiscard]] std::filesystem::path
projectDeployBuildPath(const std::filesystem::path& root);

/// The deployed game (`<root>/build/deploy`): the executable, and its
/// content under `DEPLOYED_CONTENT_DIR_NAME`.
[[nodiscard]] std::filesystem::path
projectDeployPath(const std::filesystem::path& root);

/// The executable a deploy build makes, inside its build tree.
[[nodiscard]] std::filesystem::path
deployBuiltExecutablePath(const std::filesystem::path& root);

/// The deployed game's executable, @p name plus this platform's extension.
[[nodiscard]] std::filesystem::path
executableFileName(std::string_view name);

/// Where a build of @p kind writes everything its commands print
/// (`<root>/build/logic.log`, `<root>/build/deploy.log`).
[[nodiscard]] std::filesystem::path
projectBuildLogPath(const std::filesystem::path& root, EditorBuildKind kind);

}  // namespace eng::editor
