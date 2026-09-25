#include <catch2/catch_test_macros.hpp>
#include <cstdint>
#include <engine/sim/simulation.h>
#include <functional>
#include <game/logic/game-logic.h>
#include <game/world/game-world.h>
#include <utility>
#include <vector>

using eng::game::GameLogic;
using eng::game::GameLogicWorld;
using eng::game::GameSetup;
using eng::game::GameWorld;
using eng::game::LogicEventKind;
using eng::sim::Simulation;
using eng::sim::TickHashing;
using eng::sim::TickInput;

namespace {

/// A logic whose tick is whatever a test says.
class Scripted final : public GameLogic {
public:
  explicit Scripted(std::function<void(GameLogicWorld&)> each)
    : each_(std::move(each)) {}
  void tick(GameLogicWorld& world) override { each_(world); }

private:
  /// The test's tick.
  std::function<void(GameLogicWorld&)> each_;
};

/// One player at (1.5, 1.5), an idle hostile `boss` with 5 health at
/// (6.5, 1.5), room for eight actors, and a wall at x 3–4.
GameSetup arena() {
  GameSetup setup;
  setup.spawns[0] = {1.5F, 1.5F, 0.0F};
  eng::game::ActorSpawn boss;
  boss.at = {6.5F, 1.5F, 0.0F};
  boss.behavior = "guard";
  boss.health = 5;
  boss.id = "boss";
  boss.death_blast_radius = 3.0F;
  boss.death_blast_damage = 1;
  setup.actors.push_back(boss);
  setup.obstacles.push_back({{3.0F, -2.0F, 0.0F}, {4.0F, 5.0F, 2.0F}});
  setup.actor_capacity = 8;
  return setup;
}

/// Step @p simulation @p ticks times on no input.
void run(Simulation& simulation, int ticks) {
  for (int i = 0; i < ticks; ++i) {
    (void)simulation.step(TickInput{});
  }
}

/// The kinds of every event @p world reported, tick by tick, gathered by
/// the logic into @p seen.
std::function<void(GameLogicWorld&)>
recordKinds(std::vector<LogicEventKind>& seen) {
  return [&seen](GameLogicWorld& world) {
    for (const auto& event : world.events()) {
      seen.push_back(event.kind);
    }
  };
}

}  // namespace

TEST_CASE("game logic finds an actor or a player again by its target") {
  bool found = false;
  bool crossed = true;
  Scripted logic([&](GameLogicWorld& world) {
    const auto boss = world.actorOf(world.actor(0).target);
    found = boss && boss->id == "boss";
    crossed = world.playerOf(world.actor(0).target).has_value();
  });
  GameWorld world(arena(), {}, &logic);
  Simulation simulation(world, TickHashing::ON);

  run(simulation, 1);

  CHECK(found);
  CHECK_FALSE(crossed);
}

TEST_CASE("game logic hears last tick's hurts and deaths, and where the dead "
          "fell") {
  std::vector<eng::game::LogicEvent> heard;
  Scripted logic([&](GameLogicWorld& world) {
    if (world.tick() == 0) {
      world.damage(world.actor(0).target, 2);
    } else if (world.tick() == 1) {
      world.damage(world.actor(0).target, 9);
    }
    heard.insert(heard.end(), world.events().begin(), world.events().end());
  });
  GameWorld world(arena(), {}, &logic);
  Simulation simulation(world, TickHashing::ON);

  run(simulation, 3);

  REQUIRE(heard.size() == 2);
  CHECK(heard[0].kind == LogicEventKind::ACTOR_HURT);
  CHECK(heard[1].kind == LogicEventKind::ACTOR_DIED);
  CHECK(heard[1].at.x == 6.5F);
}

TEST_CASE("an actor the logic removes is reported removed, and goes off in "
          "no blast") {
  std::vector<LogicEventKind> seen;
  const auto record = recordKinds(seen);
  Scripted logic([&](GameLogicWorld& world) {
    if (world.tick() == 0) {
      world.removeActor(world.actor(0).target);
    }
    record(world);
  });
  GameWorld world(arena(), {}, &logic);
  Simulation simulation(world, TickHashing::ON);

  run(simulation, 2);

  CHECK(seen == std::vector<LogicEventKind>{LogicEventKind::ACTOR_REMOVED});
  CHECK(world.actors().slots.size() == 0);
  CHECK(world.players().health[0] == world.players().max_health[0]);
}

namespace {

/// Spawn an actor and hurt player 1 on tick 0, then down them on tick 50,
/// past the grace a hit gives.
void spawnHurtThenDown(GameLogicWorld& world) {
  if (world.tick() == 0) {
    (void)world.spawnActor({.at = {7.5F, 3.5F, 0.0F}, .behavior = "idle"});
    world.damage(world.player(0).target, 1);
  } else if (world.tick() == 50) {
    world.damage(world.player(0).target, 999);
  }
}

/// What a logic asked of the level's walls.
struct WallAnswers {
  /// Whether it sees through the wall.
  bool through_wall = true;
  /// Whether it sees along it.
  bool along_wall = false;
  /// Whether it can stand in it.
  bool in_wall = true;
  /// Solid boxes the level has.
  uint32_t walls = 0;
};

/// Ask @p world about arena()'s wall, into @p answers.
void askAboutWalls(const GameLogicWorld& world, WallAnswers& answers) {
  answers.through_wall =
      world.lineOfSight({1.5F, 1.5F, 0.0F}, {6.5F, 1.5F, 0.0F});
  answers.along_wall =
      world.lineOfSight({1.5F, 1.5F, 0.0F}, {1.5F, 4.5F, 0.0F});
  answers.in_wall = world.walkable({3.5F, 1.5F, 0.0F});
  answers.walls = world.obstacleCount();
}

}  // namespace

TEST_CASE("game logic hears the actors it spawned, and players hurt and "
          "downed") {
  std::vector<LogicEventKind> seen;
  const auto record = recordKinds(seen);
  Scripted logic([&](GameLogicWorld& world) {
    spawnHurtThenDown(world);
    record(world);
  });
  GameWorld world(arena(), {}, &logic);
  Simulation simulation(world, TickHashing::ON);

  run(simulation, 60);

  CHECK(seen == std::vector<LogicEventKind>{LogicEventKind::ACTOR_SPAWNED,
                                            LogicEventKind::PLAYER_HURT,
                                            LogicEventKind::PLAYER_DOWNED});
}

TEST_CASE("game logic moves players and actors") {
  Scripted logic([](GameLogicWorld& world) {
    if (world.tick() == 0) {
      world.moveTo(world.player(0).target, {9.5F, 9.5F, 0.0F});
      world.moveTo(world.actor(0).target, {8.5F, 8.5F, 0.0F});
    }
  });
  GameWorld world(arena(), {}, &logic);
  Simulation simulation(world, TickHashing::ON);

  run(simulation, 1);

  CHECK(world.players().position[0].x == 9.5F);
  CHECK(world.actors().position[0].x == 8.5F);
}

TEST_CASE("game logic sets an actor's state and side") {
  bool bad_state = true;
  Scripted logic([&](GameLogicWorld& world) {
    if (world.tick() == 0) {
      bad_state = world.setActorState(world.actor(0).target, "nothing");
      REQUIRE(world.setActorState(world.actor(0).target, "return_home"));
      world.setActorFaction(world.actor(0).target, eng::game::Faction::NEUTRAL);
    }
  });
  GameWorld world(arena(), {}, &logic);
  Simulation simulation(world, TickHashing::ON);

  run(simulation, 1);

  const eng::game::ActorPool& actors = world.actors();
  CHECK_FALSE(bad_state);
  CHECK(actors.faction[0] == eng::game::Faction::NEUTRAL);
  CHECK(world.brains()[actors.brain[0]].behavior.states[actors.state[0]].id ==
        "return_home");
}

TEST_CASE("game logic sees the level's walls: line of sight, walkable "
          "floor, obstacles") {
  WallAnswers answers;
  Scripted logic([&](GameLogicWorld& world) { askAboutWalls(world, answers); });
  GameWorld world(arena(), {}, &logic);
  Simulation simulation(world, TickHashing::ON);

  run(simulation, 1);

  CHECK_FALSE(answers.through_wall);
  CHECK(answers.along_wall);
  CHECK_FALSE(answers.in_wall);
  CHECK(answers.walls == 1);
}
