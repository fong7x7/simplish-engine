#pragma once

/// @file logic-test-runner.h
/// @brief Running a project's logic tests, each on a fresh world.
/// @par Threading Main-thread-only.

#include <editor/build/editor-logic-library.h>
#include <editor/build/editor-logic-test.h>
#include <editor/deploy/logic-test-result.h>
#include <filesystem>
#include <game/logic/game-logic-factory.h>
#include <game/sdk/logic-test-case.h>
#include <span>
#include <string>
#include <vector>

namespace eng::editor {


/// The tests @p library exports — `SIMPLISH_LOGIC_TEST`s — in the order
/// they were written; none when it exports none.
[[nodiscard]] std::span<const game::sdk::LogicTestCase>
libraryTests(const EditorLogicLibrary& library);

/// Run @p test: a fresh world of its level from the content at @p content,
/// with its players and room to spawn, a fresh instance of the logic
/// @p logic makes, and its body driving it.
[[nodiscard]] LogicTestResult
runLogicTest(const game::sdk::LogicTestCase& test, game::GameLogicFactory logic,
             const std::filesystem::path& content);

/// @p results as JSON, for the editor to read back.
[[nodiscard]] std::string
logicTestResultsJson(const std::vector<LogicTestResult>& results);

}  // namespace eng::editor
