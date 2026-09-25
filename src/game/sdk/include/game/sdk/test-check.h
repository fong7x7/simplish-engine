#pragma once

/// @file test-check.h
/// @brief One expectation a logic test checked.
/// @par Threading
/// A value type; its views live as long as the call it is passed to.

#include <cstdint>
#include <string_view>

namespace eng::game::sdk {

/// What `LogicTest::expect` hands the run: whether it held, what it was,
/// and where it was checked.
struct TestCheck {
  /// Whether it held.
  bool passed = true;
  /// What was expected, in the test's words.
  std::string_view what{};
  /// The source file it was checked in.
  std::string_view file{};
  /// The line it was checked on.
  uint32_t line = 0;
};

}  // namespace eng::game::sdk
