// The horde at full load: Engine §7 budgets enemy AI and steering at
// 2.5 ms a tick for 2,000 actors, and Development §6 says a budget is
// measured, not assumed. The timing cases — the horde's tick, and a tick's
// whole path budget spent — are hidden, since they mean something only in
// an optimised build, and are what `scripts/perf-gate.sh` runs; the
// determinism case runs with the rest.

#include "support/horde-scenario.h"

#include <algorithm>
#include <catch2/catch_test_macros.hpp>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <engine/math/vec2.h>
#include <engine/sim/simulation.h>
#include <engine/sim/tick-hash-builder.h>
#include <engine/spatial/path-finder.h>
#include <game/actors/actor-workspace.h>
#include <game/world/game-world.h>
#include <vector>

using namespace eng;
using namespace eng::game;

namespace {

/// Ticks run before timing starts, so the horde has closed in.
constexpr uint64_t WARMUP_TICKS = 120;

/// Ticks timed.
constexpr uint64_t TIMED_TICKS = 600;

/// Engine §7's budget for enemy AI and steering, in milliseconds.
constexpr double AI_BUDGET_MS = 2.5;

/// What the actors' whole path budget may cost a tick, in milliseconds:
/// the horde's actors take about 1.1 ms of the AI budget's 2.5, so a tick
/// that also spends every path expansion it is allowed still fits, with
/// room to spare.
constexpr double PATH_BUDGET_MS = 1.0;

using Clock = std::chrono::steady_clock;

/// Milliseconds since @p start.
double millisecondsSince(Clock::time_point start) {
  return std::chrono::duration<double, std::milli>(Clock::now() - start)
      .count();
}

/// One phase's timings, sorted.
struct PhaseTimes {
  /// What the phase is called in the report.
  const char* name = "";
  /// Its time on each timed tick, in milliseconds.
  std::vector<double> ms{};
};

/// The value @p fraction of the way through @p sorted.
double percentile(const std::vector<double>& sorted, double fraction) {
  const auto last = static_cast<double>(sorted.size() - 1);
  const auto at = static_cast<size_t>(fraction * last);
  return sorted[at];
}

/// Print @p phase's median, 99th percentile and worst.
void report(PhaseTimes& phase) {
  std::ranges::sort(phase.ms);
  std::printf("  %-14s median %7.3f ms   p99 %7.3f ms   max %7.3f ms\n",
              phase.name, percentile(phase.ms, 0.5), percentile(phase.ms, 0.99),
              phase.ms.back());
}

/// Run @p phase, adding its time to @p times.
template <typename Phase> void timed(PhaseTimes& times, Phase phase) {
  const auto start = Clock::now();
  phase();
  times.ms.push_back(millisecondsSince(start));
}

/// Run one tick of @p world's phases by hand, adding each one's time to
/// @p times: players, actors, combat — weapon fire, projectiles and
/// damage — and compaction with hashing.
void timedTick(GameWorld& world, uint64_t tick,
               std::vector<PhaseTimes>& times) {
  const sim::TickContext context{tick, test::hordeInput(tick)};
  timed(times[0], [&] { world.playerControl(context); });
  timed(times[1], [&] { world.enemyAi(context); });
  timed(times[2], [&] {
    world.weaponFire(context);
    world.projectiles(context);
    world.damage(context);
  });
  timed(times[3], [&] {
    world.compaction(context);
    sim::TickHashBuilder hashes;
    world.hashState(hashes);
  });
}

/// Each phase's time on every tick of @p world after the warm-up.
std::vector<PhaseTimes> timedRun(GameWorld& world) {
  std::vector<PhaseTimes> times;
  for (uint64_t tick = 0; tick < WARMUP_TICKS + TIMED_TICKS; ++tick) {
    if (tick == WARMUP_TICKS || tick == 0) {
      times = {{"players"}, {"actors"}, {"combat"}, {"compact+hash"}};
    }
    timedTick(world, tick, times);
  }
  return times;
}

/// The combined hash of every tick of a run of @p ticks with @p actors.
std::vector<uint64_t> hordeHashes(uint32_t actors, uint64_t ticks) {
  GameWorld world(test::hordeSetup(actors), test::hordeContent());
  sim::Simulation simulation(world, sim::TickHashing::ON);
  std::vector<uint64_t> hashes;
  for (uint64_t tick = 0; tick < ticks; ++tick) {
    hashes.push_back(simulation.step(test::hordeInput(tick)).hash->combined);
  }
  return hashes;
}

/// A floor the size of the horde's arena with a cup forty tiles across in
/// it, open away from the goal: a search from the cup's bottom to just
/// behind it expands the whole cup before it finds the way out, so it
/// spends every expansion it is allowed.
spatial::NavGrid cupGrid() {
  static const std::vector<physics::CollisionBox> cup{
      {{-20.0F, -20.0F, 0.0F}, {20.0F, -19.0F, 2.0F}},
      {{-20.0F, -19.0F, 0.0F}, {-19.0F, 20.0F, 2.0F}},
      {{19.0F, -19.0F, 0.0F}, {20.0F, 20.0F, 2.0F}}};
  return {{.origin = {-41.0F, -41.0F}, .width = 328, .height = 328}, cup};
}

/// Time a search that spends the actors' whole path budget for a tick,
/// @p runs times over.
PhaseTimes timedBudgetSearches(int runs) {
  const spatial::NavGrid grid = cupGrid();
  spatial::PathFinder finder(grid.cellCount());
  // From (0, −17), at the cup's bottom, to (0, −24), just behind it.
  const spatial::PathRequest request{.from = {164, 96},
                                     .to = {164, 68},
                                     .max_expansions =
                                         ACTOR_PATH_BUDGET_PER_TICK};
  const spatial::PathResult first = finder.find(grid, request);
  REQUIRE(first.status == spatial::PathStatus::OVER_BUDGET);
  PhaseTimes search{"path budget"};
  for (int run = 0; run < runs; ++run) {
    timed(search, [&] { (void)finder.find(grid, request); });
  }
  return search;
}

/// The mean distance from each of @p world's actors to the nearest player.
float meanDistanceToPlayers(const GameWorld& world) {
  const ActorPool& actors = world.actors();
  const PlayerPool& players = world.players();
  float total = 0.0F;
  for (uint32_t i = 0; i < actors.slots.size(); ++i) {
    float nearest = 1.0e9F;
    for (uint32_t p = 0; p < players.slots.size(); ++p) {
      const Vec2 a{actors.position[i].x, actors.position[i].y};
      const Vec2 b{players.position[p].x, players.position[p].y};
      nearest = std::min(nearest, Vec2::distance(a, b));
    }
    total += nearest;
  }
  return total / static_cast<float>(actors.slots.size());
}

}  // namespace

TEST_CASE("a horde closes in on the players, round the pillars") {
  GameWorld world(test::hordeSetup(400), test::hordeContent());
  sim::Simulation simulation(world, sim::TickHashing::OFF);
  const float before = meanDistanceToPlayers(world);
  for (uint64_t tick = 0; tick < 300; ++tick) {
    (void)simulation.step(test::hordeInput(tick));
  }
  // Four hundred can't all stand at a player; a ring a few deep can.
  REQUIRE(meanDistanceToPlayers(world) < before * 0.4F);
}

TEST_CASE("a horde gives the same hash on every tick of every run") {
  const auto first = hordeHashes(400, 240);
  REQUIRE(hordeHashes(400, 240) == first);
}

TEST_CASE("2,000 actors stay inside the enemy AI budget", "[.][perf]") {
  GameWorld world(test::hordeSetup(test::HORDE_ACTOR_COUNT),
                  test::hordeContent());
  std::vector<PhaseTimes> times = timedRun(world);
  std::printf("horde: %u actors, %llu ticks, actor budget %.2f ms\n",
              world.actors().slots.size(),
              static_cast<unsigned long long>(TIMED_TICKS), AI_BUDGET_MS);
  for (PhaseTimes& phase : times) {
    report(phase);
  }
#ifdef NDEBUG
  REQUIRE(percentile(times[1].ms, 0.5) <= AI_BUDGET_MS);
#endif
}

TEST_CASE("a tick's whole path budget stays inside what the horde leaves",
          "[.][perf]") {
  PhaseTimes search = timedBudgetSearches(200);
  report(search);
  std::printf("  %u expansions, %.1f ns each at the median; %.2f ms allowed\n",
              ACTOR_PATH_BUDGET_PER_TICK,
              percentile(search.ms, 0.5) * 1.0e6 / ACTOR_PATH_BUDGET_PER_TICK,
              PATH_BUDGET_MS);
#ifdef NDEBUG
  REQUIRE(percentile(search.ms, 0.5) <= PATH_BUDGET_MS);
#endif
}
