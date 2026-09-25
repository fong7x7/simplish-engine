#include "host-logic-test.h"

#include <algorithm>
#include <editor/deploy/deployed-content.h>
#include <editor/deploy/logic-test-runner.h>
#include <game/sdk/logic-tests.h>
#include <nlohmann/json.hpp>

namespace eng::editor {

namespace {

  /// @p setup made ready for @p test: its players, and room for the logic
  /// to spawn into.
  game::GameSetup setupFor(game::GameSetup setup,
                           const game::sdk::LogicTestCase& test) {
    setup.player_count = static_cast<uint8_t>(
        std::clamp<unsigned>(test.players, 1, sim::MAX_PLAYERS));
    setup.actor_capacity =
        std::max(setup.actor_capacity, game::GAME_LOGIC_ACTOR_CAPACITY);
    return setup;
  }

}  // namespace

std::span<const game::sdk::LogicTestCase>
libraryTests(const EditorLogicLibrary& library) {
  // A symbol from dlsym is a void*; naming the function it is needs a cast.
  // NOLINTNEXTLINE(cppcoreguidelines-pro-type-reinterpret-cast)
  const auto list = reinterpret_cast<game::sdk::LogicTestsFn>(
      library.symbol(game::sdk::LOGIC_TESTS_SYMBOL));
  uint32_t count = 0;
  const game::sdk::LogicTestCase* tests =
      list == nullptr ? nullptr : list(&count);
  return tests == nullptr ? std::span<const game::sdk::LogicTestCase>{}
                          : std::span{tests, count};
}

LogicTestResult runLogicTest(const game::sdk::LogicTestCase& test,
                             game::GameLogicFactory logic,
                             const std::filesystem::path& content) {
  LogicTestResult result{test.name, test.level, true, 0, {}};
  const auto setup = readDeployedSetup(content, test.level);
  if (!setup || test.body == nullptr) {
    result.passed = false;
    result.failures.push_back({{}, 0, "no level " + result.level + " to play"});
    return result;
  }
  HostLogicTest run(setupFor(*setup, test), readDeployedContent(content),
                    logic);
  test.body(run);
  result.failures = run.failures();
  result.passed = result.failures.empty();
  result.ticks = run.ticks();
  return result;
}

std::string logicTestResultsJson(const std::vector<LogicTestResult>& results) {
  nlohmann::json out = nlohmann::json::array();
  for (const LogicTestResult& result : results) {
    nlohmann::json failures = nlohmann::json::array();
    for (const LogicTestFailure& failure : result.failures) {
      failures.push_back({{"file", failure.file},
                          {"line", failure.line},
                          {"message", failure.message}});
    }
    out.push_back({{"name", result.name},
                   {"level", result.level},
                   {"passed", result.passed},
                   {"ticks", result.ticks},
                   {"failures", failures}});
  }
  return out.dump(2);
}

}  // namespace eng::editor
