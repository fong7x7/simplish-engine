#include "support/sdk-rig.h"

#include <catch2/catch_test_macros.hpp>
#include <cstdint>
#include <game/sdk/event-queries.h>

using namespace eng::game;
using namespace eng::game::sdk;

namespace {

/// What a logic that hits grunt_a for 1 and grunt_b for 2 on tick 0, the
/// first credited to player 1, found of it on tick 1.
struct Tally {
  /// Hurts credited to player 1.
  uint32_t by_player = 0;
  /// Health taken from grunts by the logic.
  uint32_t taken = 0;
  /// Whether grunt_b's hurt was heard, by its exact name.
  bool heard_b = false;
  /// Whether any death was heard.
  bool heard_death = true;
};

/// Hit the grunts on tick 0; tally what was heard on tick 1.
void hitThenTally(GameLogicWorld& world, Tally& tally) {
  if (world.tick() == 0) {
    world.damage(world.actor(0).target, 1, world.player(0).target);
    world.damage(world.actor(1).target, 2);
  } else if (world.tick() == 1) {
    tally.by_player = countEvents(world, {.by = world.player(0).target});
    tally.taken = totalAmount(
        world, {.id_prefix = "grunt", .cause = LogicDamageCause::LOGIC});
    tally.heard_b =
        heard(world, {.kind = LogicEventKind::ACTOR_HURT, .id = "grunt_b"});
    tally.heard_death = heard(world, {.kind = LogicEventKind::ACTOR_DIED});
  }
}

}  // namespace

TEST_CASE("event queries pick out the last tick's events by filter") {
  Tally tally;
  test::ScriptedLogic logic(
      [&](GameLogicWorld& world) { hitThenTally(world, tally); });

  test::runLogic(logic, 2, test::sdkArena(), {});

  CHECK(tally.by_player == 1);
  CHECK(tally.taken == 3);
  CHECK(tally.heard_b);
  CHECK_FALSE(tally.heard_death);
}
