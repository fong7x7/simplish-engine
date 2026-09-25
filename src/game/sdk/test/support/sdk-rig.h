#pragma once

/// @file sdk-rig.h
/// @brief A world for the SDK's tests to run a logic in.
/// @par Threading
/// Main-thread-only.

#include <cstdint>
#include <functional>
#include <game/content/game-content.h>
#include <game/logic/game-logic.h>
#include <game/world/game-setup.h>

namespace eng::game::sdk::test {

/// A logic whose tick is whatever a test says.
class ScriptedLogic final : public GameLogic {
public:
  /// A logic running @p each every tick.
  explicit ScriptedLogic(std::function<void(GameLogicWorld&)> each);
  void tick(GameLogicWorld& world) override;

private:
  /// The test's tick.
  std::function<void(GameLogicWorld&)> each_;
};

/// One player at (1.5, 1.5); hostile `grunt_a` at (4.5, 1.5) and
/// `grunt_b` at (8.5, 1.5), and neutral `villager` at (1.5, 5.5), all
/// idle; a wall from x 12 to 13; room for 32 actors.
[[nodiscard]] GameSetup sdkArena();

/// Run @p logic in `sdkArena()` for @p ticks with player 1 holding fire,
/// aimed at +X, and say how many projectiles are then in flight.
[[nodiscard]] uint32_t runLogicFiring(GameLogic& logic, int ticks);

/// Run @p logic in a world of @p setup and @p content for @p ticks.
void runLogic(GameLogic& logic, int ticks, const GameSetup& setup,
              const GameContent& content);

}  // namespace eng::game::sdk::test
