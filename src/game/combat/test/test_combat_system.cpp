#include <catch2/catch_test_macros.hpp>
#include <game/combat/combat-system.h>
#include <vector>

using namespace eng;
using namespace eng::game;

namespace {

/// A floor 20 tiles square from (-10, -10), some boxes, some bodies, and
/// the buffers the combat phases use, stepped one tick at a time.
struct CombatRig {
  /// A rig with @p boxes for walls.
  explicit CombatRig(std::vector<physics::CollisionBox> boxes = {})
    : obstacles(std::move(boxes)), broadphase(obstacles),
      workspace(16, {.origin = {-10, -10}, .width = 80, .height = 80},
                broadphase) {}

  /// Add a body at @p at on @p side, as actor @p slot; its index.
  uint32_t addBody(Vec2 at, Faction side, uint32_t slot) {
    workspace.bodies.push_back(
        {{CombatantKind::ACTOR, {slot, 1}}, at, 0.4F, side});
    return static_cast<uint32_t>(workspace.bodies.size() - 1);
  }

  /// This tick's scene.
  CombatScene scene() {
    return {tick, obstacles, broadphase, workspace, effects};
  }

  /// Spawn what the effects ask for and step everything @p ticks ticks.
  void step(uint32_t ticks = 1) {
    indexCombatBodies(workspace);
    for (uint32_t t = 0; t < ticks; ++t) {
      spawnCombatEffects(projectiles, hazards, effects);
      effects.shots.clear();
      effects.hazards.clear();
      stepProjectiles(projectiles, scene());
      stepHazards(hazards, scene());
      compactProjectiles(projectiles);
      compactHazards(hazards);
      ++tick;
    }
  }

  /// The level's boxes.
  std::vector<physics::CollisionBox> obstacles;
  /// The boxes, bucketed.
  physics::BoxBroadphase broadphase;
  /// Bodies and scratch.
  CombatWorkspace workspace;
  /// The effects buffer.
  CombatEffects effects;
  /// Projectiles in flight.
  ProjectilePool projectiles{4};
  /// Hazards on the floor.
  HazardPool hazards{4};
  /// The next tick.
  uint64_t tick = 0;
};

/// A hostile shot from @p from moving @p velocity a tick, dealing 2.
ShotRequest shot(Vec2 from, Vec2 velocity) {
  return {from, velocity, 2, Faction::HOSTILE};
}

}  // namespace

TEST_CASE("a shot flies until it reaches someone on the other side") {
  CombatRig rig;
  rig.addBody({4.0F, 0.1F}, Faction::HOSTILE, 1);
  const uint32_t target = rig.addBody({6.0F, 0.0F}, Faction::FRIENDLY, 2);
  rig.effects.shots.push_back(shot({0, 0}, {0.5F, 0}));

  rig.step(9);
  REQUIRE(rig.effects.damage.empty());
  REQUIRE(rig.projectiles.slots.size() == 1);
  rig.step(3);
  // Passed straight through its own side, and hit the other.
  REQUIRE(rig.effects.damage.size() == 1);
  REQUIRE(rig.effects.damage[0].target.handle ==
          rig.workspace.bodies[target].who.handle);
  REQUIRE(rig.effects.damage[0].amount == 2);
  REQUIRE(rig.projectiles.slots.size() == 0);
}

TEST_CASE("a wall stops a shot before anyone behind it") {
  CombatRig rig({{{2.0F, -1.0F, 0.0F}, {2.5F, 1.0F, 2.0F}}});
  rig.addBody({4.0F, 0.0F}, Faction::FRIENDLY, 1);
  rig.effects.shots.push_back(shot({0, 0}, {0.5F, 0}));
  rig.step(20);
  REQUIRE(rig.effects.damage.empty());
  REQUIRE(rig.projectiles.slots.size() == 0);
}

TEST_CASE("a shot that reaches nobody falls after its flight") {
  CombatRig rig;
  rig.effects.shots.push_back(shot({0, 0}, {0.01F, 0}));
  rig.step(PROJECTILE_FLIGHT_TICKS - 1);
  REQUIRE(rig.projectiles.slots.size() == 1);
  rig.step();
  REQUIRE(rig.projectiles.slots.size() == 0);
}

TEST_CASE("a pool bites the other side standing in it, on its bite ticks") {
  CombatRig rig;
  rig.addBody({0.5F, 0.0F}, Faction::FRIENDLY, 1);
  rig.addBody({0.0F, 0.5F}, Faction::HOSTILE, 2);
  rig.addBody({5.0F, 0.0F}, Faction::FRIENDLY, 3);
  rig.effects.hazards.push_back({{0, 0}, 1.0F, 1, 61, Faction::HOSTILE});

  rig.step();
  REQUIRE(rig.effects.damage.size() == 1);
  rig.step(HAZARD_BITE_INTERVAL_TICKS);
  REQUIRE(rig.effects.damage.size() == 2);
  rig.step(HAZARD_BITE_INTERVAL_TICKS);
  REQUIRE(rig.effects.damage.size() == 3);
  REQUIRE(rig.hazards.slots.size() == 0);
}

TEST_CASE("a blast hurts everyone near it, on every side, but its source") {
  CombatRig rig;
  const uint32_t source = rig.addBody({0, 0}, Faction::HOSTILE, 1);
  rig.addBody({1, 0}, Faction::HOSTILE, 2);
  rig.addBody({0, 1.5F}, Faction::FRIENDLY, 3);
  rig.addBody({0, 4}, Faction::FRIENDLY, 4);
  indexCombatBodies(rig.workspace);
  rig.effects.blasts.push_back(
      {{0, 0}, 1.5F, 3, rig.workspace.bodies[source].who});

  resolveBlasts(rig.scene());

  REQUIRE(rig.effects.damage.size() == 2);
  REQUIRE(rig.effects.damage[0].amount == 3);
  REQUIRE(rig.effects.blasts.empty());
}

TEST_CASE("a shot or pool past what the pools hold is dropped") {
  CombatRig rig;
  for (int i = 0; i < 6; ++i) {
    rig.effects.shots.push_back(shot({0, 0}, {0.01F, 0}));
  }
  rig.step();
  REQUIRE(rig.projectiles.slots.size() == 4);
}
