#pragma once

/// @file editor-build-commands.h
/// @brief The commands that build a project's logic, and deploy its game.
/// @par Threading Thread-safe (pure functions).

#include <editor/build/editor-build-command.h>
#include <editor/build/editor-toolchain.h>
#include <filesystem>
#include <string>
#include <vector>

namespace eng::editor {

/// Configure and build the project at @p root's `src/` as a shared
/// library, with @p tools: the two commands Build ▸ Build Game Logic runs.
/// Configured every time, which costs next to nothing once it has been
/// done and picks up a source file the project has added since.
[[nodiscard]] std::vector<EditorBuildCommand>
logicBuildCommands(const std::filesystem::path& root,
                   const EditorToolchain& tools);

/// Configure the engine with the project at @p root's logic linked in, in
/// Release, and build the deployed game's executable: what Build ▸ Deploy
/// Game runs. The first one builds the engine from scratch, and takes
/// minutes; later ones rebuild what changed.
[[nodiscard]] std::vector<EditorBuildCommand>
deployBuildCommands(const std::filesystem::path& root,
                    const EditorToolchain& tools);

/// @p command as one line for the platform's shell, every word quoted,
/// appending all it prints to @p log.
[[nodiscard]] std::string shellLine(const EditorBuildCommand& command,
                                    const std::filesystem::path& log);

}  // namespace eng::editor
