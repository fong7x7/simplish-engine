#pragma once

/// @file host-logic-test.h
/// @brief A logic test's run: a world, stepped by the test's hand.
/// @par Threading Main-thread-only.

#include <cstdint>
#include <editor/deploy/logic-test-failure.h>
#include <engine/sim/simulation.h>
#include <engine/sim/tick-input.h>
#include <game/content/game-content.h>
#include <game/logic/game-logic-factory.h>
#include <game/logic/game-logic-instance.h>
#include <game/sdk/logic-test.h>
#include <game/world/game-setup.h>
#include <game/world/game-world.h>
#include <memory>
#include <string>
#include <vector>

namespace eng::editor {

/// `LogicTest` over a `GameWorld`: the players hold what the test says,
/// and nothing else plays them.
class HostLogicTest final : public game::sdk::LogicTest {
public:
  /// A run of @p setup and @p content with a fresh instance of the logic
  /// @p logic makes, at tick 0.
  HostLogicTest(const game::GameSetup& setup, const game::GameContent& content,
                game::GameLogicFactory logic);

  void run(uint64_t ticks) override;
  void hold(uint8_t slot, const game::sdk::TestInput& input) override;
  [[nodiscard]] const game::GameLogicWorld& world() const override;
  [[nodiscard]] bool logged(std::string_view text) const override;
  void record(const game::sdk::TestCheck& check) override;

  /// Every failed expectation.
  [[nodiscard]] const std::vector<LogicTestFailure>& failures() const {
    return failures_;
  }
  /// Ticks run.
  [[nodiscard]] uint64_t ticks() const { return simulation_.nextTick(); }

private:
  /// The logic under test.
  game::GameLogicInstance logic_;
  /// The world it runs in.
  game::GameWorld world_;
  /// What steps it.
  sim::Simulation simulation_;
  /// What each player holds.
  sim::TickInput input_{};
  /// The world as the last tick left it.
  std::unique_ptr<game::GameLogicWorld> view_;
  /// Every line the logic has logged.
  std::vector<std::string> log_;
  /// Every failed expectation.
  std::vector<LogicTestFailure> failures_;
};

}  // namespace eng::editor
