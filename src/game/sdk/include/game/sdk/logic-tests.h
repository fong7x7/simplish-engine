#pragma once

/// @file logic-tests.h
/// @brief Writing a project's logic tests: `SIMPLISH_LOGIC_TEST`.
/// @par Threading
/// Registration happens as the library loads; nothing here is called
/// after.

#include <cstdint>
#include <game/sdk/logic-test-case.h>
#include <game/sdk/logic-test.h>
#include <span>
#include <string_view>

namespace eng::game::sdk {

/// The exported name of the function listing a library's tests.
inline constexpr std::string_view LOGIC_TESTS_SYMBOL = "simplishGameLogicTests";

/// Add @p test to this library's tests. What the macros call; always
/// true, so it can initialise a static.
bool registerLogicTest(const LogicTestCase& test);

/// Every test this library registered, in the order registered.
[[nodiscard]] std::span<const LogicTestCase> logicTests();

}  // namespace eng::game::sdk

/// A logic test called @p Name, playing the level @p Level with @p Players
/// players; its body follows, with the run in `test`. Written in a source
/// listed under `TESTS` in `src/CMakeLists.txt`, which builds it into the
/// playtest library and never into a deployed game.
#define SIMPLISH_LOGIC_TEST_WITH(Name, Level, Players)                         \
  static void simplishLogicTest_##Name(::eng::game::sdk::LogicTest& test);     \
  [[maybe_unused]] static const bool simplishLogicTestRegistered_##Name =      \
      ::eng::game::sdk::registerLogicTest(                                     \
          {#Name, Level, Players, &simplishLogicTest_##Name});                 \
  static void simplishLogicTest_##Name(::eng::game::sdk::LogicTest& test)

/// A logic test called @p Name, playing the level @p Level with one player.
#define SIMPLISH_LOGIC_TEST(Name, Level)                                       \
  SIMPLISH_LOGIC_TEST_WITH(Name, Level, 1)
