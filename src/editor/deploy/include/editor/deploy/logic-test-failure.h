#pragma once

/// @file logic-test-failure.h
/// @brief One expectation of a logic test that did not hold.
/// @par Threading Thread-safe (immutable value type).

#include <cstdint>
#include <string>

namespace eng::editor {

/// Where a test's expectation failed, and what it was.
/// @thread_safety Immutable value type.
struct LogicTestFailure {
  /// The source file it was checked in; empty when the test could not run.
  std::string file;
  /// The line it was checked on; 0 when the test could not run.
  uint32_t line = 0;
  /// What was expected, or why the test could not run.
  std::string message;
};

}  // namespace eng::editor
