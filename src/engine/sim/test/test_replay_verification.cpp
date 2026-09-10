#include "support/scripted-input.h"
#include "support/spark-world.h"

#include <catch2/catch_test_macros.hpp>
#include <engine/sim/replay-codec.h>
#include <engine/sim/replay-recorder.h>
#include <engine/sim/replay-verification.h>
#include <engine/sim/simulation.h>

using eng::sim::decodeReplay;
using eng::sim::DEFAULT_CHECKPOINT_INTERVAL;
using eng::sim::encodeReplay;
using eng::sim::Replay;
using eng::sim::ReplayRecorder;
using eng::sim::Simulation;
using eng::sim::TickHashing;
using eng::sim::verifyReplay;
using eng::sim::testing::SCRIPTED_PLAYERS;
using eng::sim::testing::scriptedInput;
using eng::sim::testing::SparkWorld;

namespace {

constexpr uint64_t SEED = 777;
constexpr uint32_t CAPACITY = 64;
constexpr uint64_t RUN_TICKS = 600;

/// Plays the scripted session and records it, as a live game would.
Replay recordSession() {
  SparkWorld world(SEED, CAPACITY);
  Simulation simulation(world, TickHashing::ON);
  ReplayRecorder recorder({"sparks", 0xABCDULL, SEED, SCRIPTED_PLAYERS},
                          DEFAULT_CHECKPOINT_INTERVAL);
  for (uint64_t tick = 0; tick < RUN_TICKS; ++tick) {
    const auto input = scriptedInput(tick);
    recorder.record(input, simulation.step(input));
  }
  return recorder.finish();
}

/// Plays `replay` back into a fresh world built from `seed`.
eng::sim::ReplayVerification playBack(const Replay& replay, uint64_t seed) {
  SparkWorld world(seed, CAPACITY);
  Simulation simulation(world, TickHashing::ON);
  return verifyReplay(replay, simulation);
}

}  // namespace

TEST_CASE("A recorded session reproduces through a replay file") {
  const auto decoded = decodeReplay(encodeReplay(recordSession()));
  REQUIRE(decoded.has_value());
  const auto verification = playBack(*decoded, decoded->header.seed);
  CHECK(verification.ok());
  CHECK(verification.ticks_run == RUN_TICKS);
}

TEST_CASE("A replay of ten seconds is small") {
  CHECK(encodeReplay(recordSession()).size() < 8 * 1024);
}

TEST_CASE("A changed input is caught at the next checkpoint, by subsystem") {
  Replay replay = recordSession();
  replay.inputs[302].players[0].buttons |= 1U;
  const auto verification = playBack(replay, SEED);
  REQUIRE_FALSE(verification.ok());
  CHECK(verification.divergence->tick == 360);
  CHECK(verification.divergence->section == "sparks");
  CHECK(verification.ticks_run == 361);
}

TEST_CASE("Playing a replay against the wrong seed diverges immediately") {
  const auto verification = playBack(recordSession(), SEED + 1);
  REQUIRE_FALSE(verification.ok());
  CHECK(verification.divergence->tick == 0);
  CHECK_FALSE(verification.divergence->section.empty());
}
