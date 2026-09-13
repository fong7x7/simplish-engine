// Consequences, end to end: the presets' attacks landing on players and
// actors through the effects buffer, the damage phase and the pools, and
// the cues that report them.

#include <algorithm>
#include <catch2/catch_test_macros.hpp>
#include <engine/sim/simulation.h>
#include <game/content/character-lookup.h>
#include <game/player/player-system.h>
#include <game/world/game-world.h>

using namespace eng;
using namespace eng::game;

namespace {

/// One player standing at the origin, and @p actors.
GameSetup playerAnd(std::vector<ActorSpawn> actors) {
  GameSetup setup;
  setup.spawns[0] = {0.0F, 0.0F, 0.0F};
  setup.actors = std::move(actors);
  return setup;
}

/// A world of @p setup stepped @p ticks idle ticks.
void run(GameWorld& world, uint64_t ticks) {
  sim::Simulation simulation(world, sim::TickHashing::OFF);
  for (uint64_t t = 0; t < ticks; ++t) {
    (void)simulation.step({});
  }
}

/// An actor running @p behavior at @p x, 0, facing +X — toward the player
/// from the negative side.
ActorSpawn at(float x, const char* behavior) {
  return {.at = {x, 0.0F, 0.0F}, .behavior = behavior};
}

}  // namespace

TEST_CASE("a swarmer bites a lone player down, a segment a grace, and the "
          "run is over") {
  GameWorld world(playerAnd({at(3.0F, "chase")}), {});
  run(world, 60);
  REQUIRE(world.players().health[0] < DEFAULT_CHARACTER_HEALTH);
  REQUIRE_FALSE(world.runOver());
  run(world, PLAYER_HURT_GRACE_TICKS * DEFAULT_CHARACTER_HEALTH + 60);
  REQUIRE(world.players().health[0] == 0);
  REQUIRE(world.players().out[0] == 1);
  REQUIRE(world.runOver());
}

TEST_CASE("a bloater bursts beside the player, hurting its own side too") {
  GameWorld world(playerAnd({at(4.0F, "bloater"), at(1.2F, "idle")}), {});
  run(world, 240);
  REQUIRE(world.players().health[0] == DEFAULT_CHARACTER_HEALTH - 2);
  // The bloater is gone, and the idler beside the player took the blast.
  REQUIRE(world.actors().slots.size() == 1);
  REQUIRE(world.actors().health[0] == ACTOR_DEFAULT_HEALTH - 2);
}

TEST_CASE("a skirmisher's volleys fly and land on the player") {
  GameWorld world(playerAnd({at(-5.0F, "skirmisher")}), {});
  bool saw_a_shot = false;
  sim::Simulation simulation(world, sim::TickHashing::OFF);
  for (int t = 0; t < 400; ++t) {
    (void)simulation.step({});
    saw_a_shot = saw_a_shot || world.projectilePool().slots.size() > 0;
  }
  REQUIRE(saw_a_shot);
  REQUIRE(world.players().health[0] < DEFAULT_CHARACTER_HEALTH);
}

TEST_CASE("a spitter's pool lies where the player stood, and bites them") {
  GameWorld world(playerAnd({at(-7.0F, "spitter")}), {});
  bool saw_a_pool = false;
  sim::Simulation simulation(world, sim::TickHashing::OFF);
  for (int t = 0; t < 300; ++t) {
    (void)simulation.step({});
    saw_a_pool = saw_a_pool || world.hazardPool().slots.size() > 0;
  }
  REQUIRE(saw_a_pool);
  REQUIRE(world.players().health[0] < DEFAULT_CHARACTER_HEALTH);
}

TEST_CASE("a defender takes on a hostile actor and kills it") {
  GameSetup setup = playerAnd({{.at = {-6.0F, 0.0F, 0.0F},
                                .behavior = "defender",
                                .faction = Faction::FRIENDLY},
                               {.at = {-2.0F, 3.0F, 0.0F},
                                .behavior = "idle",
                                .faction = Faction::HOSTILE}});
  GameWorld world(setup, {});
  run(world, 600);
  REQUIRE(world.actors().slots.size() == 1);
  REQUIRE(world.actors().faction[0] == Faction::FRIENDLY);
}

namespace {

/// Every cue @p ticks ticks of @p world make, in order.
std::vector<CombatCue> cuesOver(GameWorld& world, int ticks) {
  std::vector<CombatCue> cues;
  sim::Simulation simulation(world, sim::TickHashing::OFF);
  for (int t = 0; t < ticks; ++t) {
    (void)simulation.step({});
    const auto tick = world.combatCues();
    cues.insert(cues.end(), tick.begin(), tick.end());
  }
  return cues;
}

/// How many of @p cues are of @p kind.
size_t countOf(const std::vector<CombatCue>& cues, CombatCueKind kind) {
  return static_cast<size_t>(
      std::count_if(cues.begin(), cues.end(),
                    [kind](const CombatCue& cue) { return cue.kind == kind; }));
}

}  // namespace

TEST_CASE("a skirmisher's volleys are cued as they fire and as they land") {
  GameWorld world(playerAnd({at(-5.0F, "skirmisher")}), {});
  const std::vector<CombatCue> cues = cuesOver(world, 400);

  REQUIRE(countOf(cues, CombatCueKind::SHOT_FIRED) > 0);
  REQUIRE(countOf(cues, CombatCueKind::SHOT_HIT_BODY) > 0);
  // Every shot that landed was fired first, and none is cued twice.
  REQUIRE(countOf(cues, CombatCueKind::SHOT_HIT_BODY) +
              countOf(cues, CombatCueKind::SHOT_HIT_WALL) <=
          countOf(cues, CombatCueKind::SHOT_FIRED));
  REQUIRE(cues.front().kind == CombatCueKind::SHOT_FIRED);
  REQUIRE(cues.front().side == Faction::HOSTILE);
}

TEST_CASE("a bloater's blast is cued on the tick it goes off, and only then") {
  GameWorld world(playerAnd({at(4.0F, "bloater")}), {});
  sim::Simulation simulation(world, sim::TickHashing::OFF);
  int blast_ticks = 0;
  for (int t = 0; t < 400; ++t) {
    (void)simulation.step({});
    const auto cues = world.combatCues();
    blast_ticks += std::any_of(cues.begin(), cues.end(), [](const auto& c) {
      return c.kind == CombatCueKind::BLAST;
    });
  }
  REQUIRE(blast_ticks == 1);
}

TEST_CASE("the same run cues the same things in the same order") {
  GameWorld first(playerAnd({at(-5.0F, "skirmisher")}), {});
  GameWorld second(playerAnd({at(-5.0F, "skirmisher")}), {});
  const std::vector<CombatCue> a = cuesOver(first, 300);
  const std::vector<CombatCue> b = cuesOver(second, 300);

  REQUIRE(a.size() == b.size());
  for (size_t i = 0; i < a.size(); ++i) {
    REQUIRE(a[i].kind == b[i].kind);
    REQUIRE(a[i].at.x == b[i].at.x);
    REQUIRE(a[i].at.y == b[i].at.y);
  }
}
