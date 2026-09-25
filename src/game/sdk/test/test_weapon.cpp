#include "support/sdk-rig.h"

#include <catch2/catch_test_macros.hpp>
#include <game/sdk/entity-data.h>
#include <game/sdk/player-input.h>
#include <game/sdk/weapon.h>

using namespace eng::game;
using namespace eng::game::sdk;

namespace {

/// Fires @p weapon for player 1 every tick they hold fire, counting pulls.
class Trigger final : public GameLogic {
public:
  explicit Trigger(Weapon weapon) : weapon_(weapon) {}
  void tick(GameLogicWorld& world) override {
    pulls += fireWeapon(world, world.player(0), weapon_, cooldown_) ? 1 : 0;
    held = held || firing(world, world.player(0));
  }
  /// Pulls that fired.
  int pulls = 0;
  /// Whether fire was ever held.
  bool held = false;

private:
  /// What it fires.
  Weapon weapon_;
  /// When it can fire again.
  Cooldown cooldown_{};
};

}  // namespace

TEST_CASE("a weapon fires while fire is held, as often as it refires") {
  Trigger rifle({.refire_ticks = 8});

  // The first shots have struck grunt_a, three tiles off, by the end.
  (void)test::runLogicFiring(rifle, 17);

  CHECK(rifle.held);
  CHECK(rifle.pulls == 3);
}

TEST_CASE("a weapon fires its pellets in one pull") {
  Trigger shotgun({.refire_ticks = 60, .pellets = 5, .spread_degrees = 30});

  CHECK(test::runLogicFiring(shotgun, 1) == 5);
  CHECK(shotgun.pulls == 1);
}

TEST_CASE("a weapon fires nothing while fire is not held") {
  Trigger rifle({});
  test::ScriptedLogic logic(
      [&rifle](GameLogicWorld& world) { rifle.tick(world); });

  test::runLogic(logic, 10, test::sdkArena(), {});

  CHECK_FALSE(rifle.held);
  CHECK(rifle.pulls == 0);
}
