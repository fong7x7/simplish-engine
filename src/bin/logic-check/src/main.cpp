#include <chrono>
#include <cstdlib>
#include <editor/deploy/logic-check.h>
#include <iostream>
#include <string_view>
#include <thread>
#include <vector>

namespace {

/// Longest a check may take before it is taken to have hung. Wall-clock
/// time, on purpose: it watches the simulation from outside, and nothing
/// it measures reaches a tick.
constexpr std::chrono::seconds CHECK_LIMIT{60};

/// Give up on the check, and say why, once `CHECK_LIMIT` has passed.
void watchdog() {
  std::this_thread::sleep_for(CHECK_LIMIT);
  std::cout << "error: the game logic did not finish its check in "
            << CHECK_LIMIT.count() << " s: does a loop in it never end?\n"
            << std::flush;
  std::_Exit(3);
}

}  // namespace

/// Usage: simplish-logic-check --library PATH --content DIR [--ticks N]
///
/// What the editor runs after every Build Game Logic, before it loads the
/// library it built: the logic, run in a process of its own, so a crash or
/// a hang in it fails the build instead of taking the editor down.
int main(int argc, char** argv) {  // NOLINT(bugprone-exception-escape)
  const std::vector<std::string_view> args(argv + 1, argv + argc);
  const auto options = eng::editor::parseLogicCheckArgs(args);
  if (!options) {
    std::cerr << "usage: simplish-logic-check --library PATH --content DIR "
                 "[--ticks N]\n";
    return 2;
  }
  std::thread(watchdog).detach();
  return eng::editor::runLogicCheck(*options, std::cout);
}
