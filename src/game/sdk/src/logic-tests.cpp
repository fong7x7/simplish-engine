#include <game/logic/game-logic-entry.h>
#include <game/sdk/logic-tests.h>
#include <vector>

namespace eng::game::sdk {

namespace {

  /// Every test registered, in order. A function's static, so it exists
  /// before the first registration whatever order statics start in.
  std::vector<LogicTestCase>& registry() {
    static std::vector<LogicTestCase> tests;
    return tests;
  }

}  // namespace

bool registerLogicTest(const LogicTestCase& test) {
  registry().push_back(test);
  return true;
}

std::span<const LogicTestCase> logicTests() {
  return registry();
}

}  // namespace eng::game::sdk

// What `simplish-logic-check` finds a library's tests by. Every library the
// SDK is compiled into exports it; one with no tests lists none.
SIMPLISH_GAME_LOGIC_EXPORT const eng::game::sdk::LogicTestCase*
simplishGameLogicTests(uint32_t* count) {
  const auto tests = eng::game::sdk::logicTests();
  *count = static_cast<uint32_t>(tests.size());
  return tests.data();
}
