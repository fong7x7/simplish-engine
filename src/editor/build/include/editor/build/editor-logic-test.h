#pragma once

/// @file editor-logic-test.h
/// @brief How one of the project's logic tests went, as the editor keeps it.
/// @par Threading Thread-safe (value type; the reader is a pure function).

#include <cstdint>
#include <editor/build/editor-build-diagnostic.h>
#include <string>
#include <string_view>
#include <vector>

namespace eng::editor {

/// The file, beside the logic check's content, its tests' results are
/// written to and read back from.
inline constexpr std::string_view LOGIC_TEST_RESULTS_FILE = "tests.json";

/// One `SIMPLISH_LOGIC_TEST`'s outcome in the last logic build.
/// @thread_safety Immutable value type.
struct EditorLogicTest {
  /// Its name.
  std::string name;
  /// The level it played.
  std::string level;
  /// Whether every expectation held.
  bool passed = true;
  /// Ticks it ran.
  uint64_t ticks = 0;
  /// Each expectation that failed, where it was checked — or why the test
  /// could not run.
  std::vector<EditorBuildDiagnostic> failures;
};

/// The results the logic check wrote as @p text; none when it is not
/// results.
[[nodiscard]] std::vector<EditorLogicTest>
parseLogicTestResults(std::string_view text);

}  // namespace eng::editor
