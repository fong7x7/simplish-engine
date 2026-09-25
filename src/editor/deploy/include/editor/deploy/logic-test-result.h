#pragma once

/// @file logic-test-result.h
/// @brief How one of a project's logic tests went.
/// @par Threading Thread-safe (immutable value type).

#include <cstdint>
#include <editor/deploy/logic-test-failure.h>
#include <string>
#include <vector>

namespace eng::editor {

/// One logic test's outcome.
/// @thread_safety Immutable value type.
struct LogicTestResult {
  /// Its name.
  std::string name;
  /// The level it played.
  std::string level;
  /// Whether every expectation held.
  bool passed = true;
  /// Ticks it ran.
  uint64_t ticks = 0;
  /// Each failed expectation, or why it could not run.
  std::vector<LogicTestFailure> failures;
};

}  // namespace eng::editor
