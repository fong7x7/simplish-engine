#include <catch2/catch_test_macros.hpp>
#include <cstdint>
#include <engine/sim/simulation.h>
#include <functional>
#include <game/content/enemy-definition.h>
#include <game/logic/game-logic.h>
#include <game/world/game-world.h>
#include <string>
#include <utility>

using eng::game::GameLogic;
using eng::game::GameLogicHash;
using eng::game::GameLogicWorld;
using eng::game::GameSetup;
using eng::game::GameWorld;
using eng::game::LogicTargetKind;
using eng::game::RunOutcome;
using eng::sim::Simulation;
using eng::sim::TickHashing;
using eng::sim::TickInput;

namespace {

/// A logic whose tick is whatever a test says, counting its calls.
class Scripted final : public GameLogic {
public:
  explicit Scripted(std::function<void(GameLogicWorld&)> each)
    : each_(std::move(each)) {}

  void start([[maybe_unused]] GameLogicWorld& world) override { ++starts; }
  void tick(GameLogicWorld& world) override {
    ++ticks;
    each_(world);
  }
  void hashState(GameLogicHash& hash) const override { hash.add(hashed); }

  /// Calls of `start`.
  int starts = 0;
  /// Calls of `tick`.
  int ticks = 0;
  /// What `hashState` folds in.
  uint32_t hashed = 0;

private:
  /// The test's tick.
  std::function<void(GameLogicWorld&)> each_;
};

/// One player, and one idle hostile actor named "boss" with 5 health.
GameSetup withBoss() {
  GameSetup setup;
  setup.spawns[0] = {1.5F, 1.5F, 0.0F};
  eng::game::ActorSpawn boss;
  boss.at = {6.5F, 1.5F, 0.0F};
  boss.behavior = "idle";
  boss.health = 5;
  boss.id = "boss";
  setup.actors.push_back(boss);
  return setup;
}

/// Step @p simulation @p ticks times on no input, returning the last hash.
uint64_t run(Simulation& simulation, int ticks) {
  uint64_t hash = 0;
  for (int i = 0; i < ticks; ++i) {
    hash = simulation.step(TickInput{}).hash->combined;
  }
  return hash;
}

}  // namespace

TEST_CASE("game logic starts once and then runs every tick") {
  Scripted logic([](GameLogicWorld&) {});
  GameWorld world(withBoss(), {}, &logic);
  Simulation simulation(world, TickHashing::ON);

  (void)run(simulation, 3);

  REQUIRE(logic.starts == 1);
  REQUIRE(logic.ticks == 3);
  REQUIRE(world.hasLogic());
}

TEST_CASE("game logic reads actors by the name the level gave them") {
  std::string seen;
  Scripted logic([&](GameLogicWorld& world) {
    seen = std::string(world.actor(0).id) + "/" +
           std::string(world.actor(0).state);
  });
  GameWorld world(withBoss(), {}, &logic);
  Simulation simulation(world, TickHashing::ON);

  (void)run(simulation, 1);

  REQUIRE(seen == "boss/idle");
}

TEST_CASE("game logic's damage lands at the end of its tick, and kills") {
  uint16_t health_seen = 0;
  Scripted logic([&](GameLogicWorld& world) {
    health_seen = world.actor(0).health;
    world.damage(world.actor(0).target, 2);
  });
  GameWorld world(withBoss(), {}, &logic);
  Simulation simulation(world, TickHashing::ON);

  (void)run(simulation, 1);
  REQUIRE(health_seen == 5);
  REQUIRE(world.actors().health[0] == 3);
  (void)run(simulation, 2);
  REQUIRE(world.actors().slots.size() == 0);
}

TEST_CASE("game logic heals up to a full bar and no further") {
  Scripted logic([](GameLogicWorld& world) {
    if (world.tick() == 0) {
      world.damage(world.actor(0).target, 3);
    } else {
      world.heal(world.actor(0).target, 9);
    }
  });
  GameWorld world(withBoss(), {}, &logic);
  Simulation simulation(world, TickHashing::ON);

  (void)run(simulation, 1);
  REQUIRE(world.actors().health[0] == 2);
  (void)run(simulation, 1);
  REQUIRE(world.actors().health[0] == 5);
}

TEST_CASE("game logic ends the run, and the first ending stands") {
  Scripted logic([](GameLogicWorld& world) {
    world.endRun(world.tick() == 0 ? RunOutcome::WON : RunOutcome::LOST);
  });
  GameWorld world(withBoss(), {}, &logic);
  Simulation simulation(world, TickHashing::ON);

  REQUIRE(world.outcome() == RunOutcome::PLAYING);
  (void)run(simulation, 2);

  REQUIRE(world.runOver());
  REQUIRE(world.outcome() == RunOutcome::WON);
}

TEST_CASE("a world with game logic hashes it in a last section") {
  Scripted logic([](GameLogicWorld&) {});
  GameWorld world(withBoss(), {}, &logic);
  Simulation simulation(world, TickHashing::ON);

  const auto hash = simulation.step(TickInput{}).hash;

  REQUIRE(hash->section_count == 7);
  REQUIRE(hash->sections[6].name == "logic");
}

TEST_CASE("game logic's own state is part of the tick hash") {
  Scripted first([](GameLogicWorld&) {});
  Scripted second([](GameLogicWorld&) {});
  second.hashed = 1;
  GameWorld a(withBoss(), {}, &first);
  GameWorld b(withBoss(), {}, &second);
  Simulation step_a(a, TickHashing::ON);
  Simulation step_b(b, TickHashing::ON);

  REQUIRE(run(step_a, 1) != run(step_b, 1));
}

TEST_CASE("game logic draws the same numbers on the same seed") {
  std::vector<uint32_t> drawn[2];
  for (auto& numbers : drawn) {
    Scripted logic(
        [&](GameLogicWorld& w) { numbers.push_back(w.random(100)); });
    GameWorld world(withBoss(), {}, &logic);
    Simulation simulation(world, TickHashing::ON);
    (void)run(simulation, 4);
  }

  REQUIRE(drawn[0] == drawn[1]);
  REQUIRE(drawn[0].size() == 4);
}

TEST_CASE("game logic's draws leave the actors' stream where it was") {
  Scripted draws([](GameLogicWorld& w) { (void)w.random(100); });
  Scripted quiet([](GameLogicWorld&) {});
  GameWorld drawing(withBoss(), {}, &draws);
  GameWorld still(withBoss(), {}, &quiet);
  Simulation step_drawing(drawing, TickHashing::ON);
  Simulation step_still(still, TickHashing::ON);

  const auto a = step_drawing.step(TickInput{}).hash;
  const auto b = step_still.step(TickInput{}).hash;

  REQUIRE(a->sections[5].name == "ai_rng");
  REQUIRE(a->sections[5].hash == b->sections[5].hash);
  REQUIRE(a->sections[6].hash != b->sections[6].hash);
}

TEST_CASE("what game logic says is kept until it is taken") {
  Scripted logic([](GameLogicWorld& world) {
    world.log("tick " + std::to_string(world.tick()));
  });
  GameWorld world(withBoss(), {}, &logic);
  Simulation simulation(world, TickHashing::ON);

  (void)run(simulation, 2);

  REQUIRE(world.takeLogicLog() == std::vector<std::string>{"tick 0", "tick 1"});
  REQUIRE(world.takeLogicLog().empty());
}

TEST_CASE("game logic sees players as targets it can hurt") {
  LogicTargetKind kind = LogicTargetKind::ACTOR;
  Scripted logic([&](GameLogicWorld& world) {
    kind = world.player(0).target.kind;
    world.damage(world.player(0).target, 1);
  });
  GameWorld world(withBoss(), {}, &logic);
  Simulation simulation(world, TickHashing::ON);

  (void)run(simulation, 1);

  REQUIRE(kind == LogicTargetKind::PLAYER);
  REQUIRE(world.players().health[0] == world.players().max_health[0] - 1);
}

namespace {

/// `withBoss`, with room for @p capacity actors in all.
GameSetup roomFor(uint32_t capacity) {
  GameSetup setup = withBoss();
  setup.actor_capacity = capacity;
  return setup;
}

/// Content with one enemy archetype, `grunt`.
eng::game::GameContent withGrunt() {
  eng::game::GameContent content;
  eng::game::EnemyDefinition grunt;
  grunt.id = "grunt";
  grunt.model = "mesh:grunt";
  grunt.health = 7;
  grunt.behavior = "chase";
  content.enemies.push_back(grunt);
  return content;
}

}  // namespace

TEST_CASE("game logic spawns an actor that is read from the next tick on") {
  uint32_t seen_on_spawn = 0;
  Scripted logic([&](GameLogicWorld& world) {
    if (world.tick() == 0) {
      seen_on_spawn = world.actorCount();
      REQUIRE(world.spawnActor(
          {.at = {3.5F, 3.5F, 0.0F}, .behavior = "wander", .id = "pet"}));
    }
  });
  GameWorld world(roomFor(4), {}, &logic);
  Simulation simulation(world, TickHashing::ON);

  (void)run(simulation, 2);

  REQUIRE(seen_on_spawn == 1);
  REQUIRE(world.actors().slots.size() == 2);
  REQUIRE(world.actorId(1) == "pet");
  REQUIRE(world.actorSpawned(1));
  REQUIRE_FALSE(world.actorSpawned(0));
}

TEST_CASE("game logic spawns the project's enemy archetypes by id") {
  bool unknown = true;
  Scripted logic([&](GameLogicWorld& world) {
    if (world.tick() == 0) {
      REQUIRE(world.spawnEnemy("grunt", {4.5F, 1.5F, 0.0F}, "g1"));
      unknown = world.spawnEnemy("nobody", {4.5F, 1.5F, 0.0F}, "g2");
    }
  });
  GameWorld world(roomFor(4), withGrunt(), &logic);
  Simulation simulation(world, TickHashing::ON);

  (void)run(simulation, 1);

  REQUIRE_FALSE(unknown);
  REQUIRE(world.actors().slots.size() == 2);
  REQUIRE(world.actors().max_health[1] == 7);
  REQUIRE(world.actorModel(1) == "mesh:grunt");
}

TEST_CASE("game logic spawns no more than the run has room for") {
  std::vector<bool> spawned;
  uint32_t room_before = 0;
  Scripted logic([&](GameLogicWorld& world) {
    if (world.tick() == 0) {
      room_before = world.actorRoom();
      for (int i = 0; i < 3; ++i) {
        spawned.push_back(world.spawnActor({.at = {3.5F, 3.5F, 0.0F}}));
      }
    }
  });
  GameWorld world(roomFor(3), {}, &logic);
  Simulation simulation(world, TickHashing::ON);

  (void)run(simulation, 1);

  REQUIRE(room_before == 2);
  REQUIRE(spawned == std::vector<bool>{true, true, false});
  REQUIRE(world.actors().slots.size() == 3);
}

TEST_CASE("a run with no room spawns nothing") {
  bool spawned = true;
  Scripted logic([&](GameLogicWorld& world) {
    spawned = world.spawnActor({.at = {3.5F, 3.5F, 0.0F}});
  });
  GameWorld world(withBoss(), {}, &logic);
  Simulation simulation(world, TickHashing::ON);

  (void)run(simulation, 1);

  REQUIRE_FALSE(spawned);
}

TEST_CASE("an actor spawned into a level with none of its own finds its way") {
  GameSetup setup;
  setup.spawns[0] = {1.5F, 1.5F, 0.0F};
  setup.actor_capacity = 8;
  Scripted logic([](GameLogicWorld& world) {
    if (world.tick() == 0) {
      (void)world.spawnActor({.at = {8.5F, 1.5F, 0.0F}, .behavior = "chase"});
    }
  });
  GameWorld world(setup, {}, &logic);
  Simulation simulation(world, TickHashing::ON);

  (void)run(simulation, 60);

  REQUIRE(world.actors().slots.size() == 1);
  REQUIRE(world.actors().position[0].x < 8.0F);
}
