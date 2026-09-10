// The determinism test Development REQUIREMENTS §5.2 requires of every
// simulation system: the same scenario, run repeatedly, must produce the
// same tick hash on every tick. SparkWorld stands in for the game until
// src/game/ exists; each game system gets a test of this shape.

#include "support/scripted-input.h"
#include "support/spark-world.h"

#include <catch2/catch_test_macros.hpp>
#include <cstdint>
#include <engine/sim/simulation.h>
#include <vector>

using eng::sim::Simulation;
using eng::sim::TickHashing;
using eng::sim::TickInput;
using eng::sim::testing::scriptedInput;
using eng::sim::testing::SparkWorld;

namespace {

/// Ticks per run: ten seconds of play.
constexpr uint64_t RUN_TICKS = 600;

/// Spark capacity: enough that the pool fills and empties during a run.
constexpr uint32_t CAPACITY = 64;

/// How a run deviates from the script, if at all.
struct Variation {
  /// Session seed.
  uint64_t seed = 1234;
  /// Cosmetic RNG draws per tick.
  uint32_t cosmetic_draws = 0;
  /// A tick on which player 0 also fires, or none.
  uint64_t extra_shot_tick = UINT64_MAX;
};

/// The combined tick hash of every tick of a run.
std::vector<uint64_t> runHashes(const Variation& variation) {
  SparkWorld world(variation.seed, CAPACITY);
  world.setCosmeticDrawsPerTick(variation.cosmetic_draws);
  Simulation simulation(world, TickHashing::ON);
  std::vector<uint64_t> hashes;
  for (uint64_t tick = 0; tick < RUN_TICKS; ++tick) {
    TickInput input = scriptedInput(tick);
    if (tick == variation.extra_shot_tick) {
      input.players[0].buttons |= 1U;
    }
    hashes.push_back(simulation.step(input).hash->combined);
  }
  return hashes;
}

/// The first tick two runs disagree on, or RUN_TICKS when they never do.
std::size_t firstDifference(const std::vector<uint64_t>& a,
                            const std::vector<uint64_t>& b) {
  std::size_t tick = 0;
  while (tick < a.size() && a[tick] == b[tick]) {
    ++tick;
  }
  return tick;
}

}  // namespace

TEST_CASE("The same seed and inputs give the same hash on every tick") {
  const auto first = runHashes({});
  for (int run = 0; run < 3; ++run) {
    REQUIRE(runHashes({}) == first);
  }
}

TEST_CASE("The scenario exercises the pool, so the test proves something") {
  SparkWorld world(1234, CAPACITY);
  Simulation simulation(world, TickHashing::OFF);
  uint32_t most = 0;
  for (uint64_t tick = 0; tick < RUN_TICKS; ++tick) {
    (void)simulation.step(scriptedInput(tick));
    most = std::max(most, world.sparkCount());
  }
  CHECK(most > CAPACITY / 2);
  CHECK(world.sparkCount() < most);
}

TEST_CASE("A different seed gives a different run") {
  const auto a = runHashes({});
  const auto b = runHashes({.seed = 99});
  CHECK(firstDifference(a, b) < 10);
}

TEST_CASE("One changed input diverges the run from that tick on") {
  const auto scripted = runHashes({});
  const auto changed = runHashes({.extra_shot_tick = 302});
  CHECK(firstDifference(scripted, changed) == 302);
}

TEST_CASE("Cosmetic RNG draws never reach the hash") {
  CHECK(runHashes({.cosmetic_draws = 17}) == runHashes({}));
}
