#pragma once

/// @file editor-logic-source.h
/// @brief A project's own game logic source: making it, and finding it.
/// @par Threading Main-thread-only (touches the disk).

#include <editor/build/editor-logic-scaffold.h>
#include <filesystem>
#include <string>

namespace eng::editor {

/// The name of the file every project's game logic is built from, in its
/// `src/`: the one the engine's build and the editor's both read.
inline constexpr const char* LOGIC_CMAKE_FILE_NAME = "CMakeLists.txt";

/// The file the scaffold's example logic is written to.
inline constexpr const char* LOGIC_EXAMPLE_FILE_NAME = "game-logic.cpp";

/// Whether the project at @p root has game logic: a `src/CMakeLists.txt`.
[[nodiscard]] bool projectHasLogic(const std::filesystem::path& root);

/// Give the project at @p root game logic to start from — a build file
/// and a small example in `src/` — unless it has some. Also marks
/// `build/` as ignored, with a `.gitignore` of its own inside it.
[[nodiscard]] EditorLogicScaffold
scaffoldProjectLogic(const std::filesystem::path& root);

/// Whether any source under the project at @p root's `src/` was written
/// after its logic library was built — or there is source and no library
/// at all. What warns that Play is about to run old rules.
[[nodiscard]] bool projectLogicStale(const std::filesystem::path& root);

/// The scaffold's `src/CMakeLists.txt`.
[[nodiscard]] std::string logicScaffoldCMake();

/// The scaffold's example logic.
[[nodiscard]] std::string logicScaffoldSource();

}  // namespace eng::editor
