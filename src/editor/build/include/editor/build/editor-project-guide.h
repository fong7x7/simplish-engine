#pragma once

/// @file editor-project-guide.h
/// @brief The guide a game project carries for the agents that work in it.
/// @par Threading Main-thread-only (touches the disk).

#include <filesystem>
#include <string>

namespace eng::editor {

/// The guide's file, at the top of a project: what Claude Code, and the
/// other agents that read it, load first.
inline constexpr const char* PROJECT_GUIDE_FILE_NAME = "CLAUDE.md";

/// A one-line pointer to the guide, for agents that look for this name.
inline constexpr const char* PROJECT_AGENTS_FILE_NAME = "AGENTS.md";

/// The guide for a project built with the engine at @p engine_root: the
/// project's layout, the build, play and deploy loop through the editor's
/// agent tools, the game SDK and the rules its logic keeps, and where the
/// engine's own documents are.
[[nodiscard]] std::string
projectAgentGuide(const std::filesystem::path& engine_root);

/// Write the guide, and its pointer, into the project at @p root, unless
/// they are there — a project's own guide is never written over. False
/// when one could not be written.
bool writeProjectAgentGuide(const std::filesystem::path& root,
                            const std::filesystem::path& engine_root);

}  // namespace eng::editor
