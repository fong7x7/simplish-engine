// The determinism test Development REQUIREMENTS §5.2 requires of every
// simulation system, for the game's own world: the same setup and the same
// inputs give the same tick hash on every tick, and a replay of a run
// verifies against a fresh world.

#include <catch2/catch_test_macros.hpp>
#include <cstdint>
#include <engine/input/player-input-builder.h>
#include <engine/sim/replay-codec.h>
#include <engine/sim/replay-recorder.h>
#include <engine/sim/replay-verification.h>
#include <engine/sim/simulation.h>
#include <game/world/game-world.h>
#include <vector>

using eng::game::GameSetup;
using eng::game::GameWorld;
using eng::sim::Simulation;
using eng::sim::TickHashing;
using eng::sim::TickInput;

namespace {

constexpr uint64_t RUN_TICKS = 600;

GameSetup fourPlayers() {
  GameSetup setup;
  setup.player_count = 4;
  for (uint8_t slot = 0; slot < 4; ++slot) {
    setup.spawns[slot] = {static_cast<float>(slot) * 2.0F + 0.5F, 0.5F, 0.0F};
  }
  // Props for them to run into, so collision is part of what is proved
  // deterministic.
  for (int i = 0; i < 6; ++i) {
    const auto x = static_cast<float>(i) * 1.5F - 2.0F;
    setup.obstacles.push_back({{x, 2.0F, 0.0F}, {x + 1.0F, 3.0F, 1.0F}});
  }
  // Characters of different speeds, and one left to the default, so the
  // stats a player takes from content are part of it too.
  setup.characters = {"scout", "tank", "", "scout"};
  return setup;
}

eng::game::GameContent twoCharacters() {
  eng::game::GameContent content;
  content.characters.push_back({"scout", "Scout", "", 7.25F, 3});
  content.characters.push_back({"tank", "Tank", "", 3.5F, 9});
  return content;
}

/// Four players pushing their sticks around in a pattern of their own.
TickInput scripted(uint64_t tick) {
  TickInput input;
  for (uint64_t slot = 0; slot < 4; ++slot) {
    const float phase = static_cast<float>((tick + slot * 37) % 240) / 120.0F;
    input.players[slot].move_x = eng::input::quantizeInputAxis(phase - 1.0F);
    input.players[slot].move_y =
        eng::input::quantizeInputAxis(tick % 90 < 45 ? 1.0F : -0.5F);
    input.players[slot].aim_x = eng::input::quantizeInputAxis(1.0F - phase);
  }
  return input;
}

std::vector<uint64_t> runHashes(uint64_t perturbed_tick = UINT64_MAX) {
  GameWorld world(fourPlayers(), twoCharacters());
  Simulation simulation(world, TickHashing::ON);
  std::vector<uint64_t> hashes;
  for (uint64_t tick = 0; tick < RUN_TICKS; ++tick) {
    TickInput input = scripted(tick);
    if (tick == perturbed_tick) {
      input.players[2].move_x = 1;
    }
    hashes.push_back(simulation.step(input).hash->combined);
  }
  return hashes;
}

}  // namespace

TEST_CASE("the game world gives the same hash on every tick of every run") {
  const auto first = runHashes();
  REQUIRE(runHashes() == first);
  REQUIRE(runHashes() == first);
}

TEST_CASE("one changed input diverges the game world from that tick") {
  const auto scripted_run = runHashes();
  const auto changed = runHashes(250);
  size_t tick = 0;
  while (tick < scripted_run.size() && scripted_run[tick] == changed[tick]) {
    ++tick;
  }
  REQUIRE(tick == 250);
}

TEST_CASE("a recorded game run verifies against a fresh world") {
  GameWorld world(fourPlayers(), twoCharacters());
  Simulation simulation(world, TickHashing::ON);
  eng::sim::ReplayRecorder recorder({"main", 0, 0, 4},
                                    eng::sim::DEFAULT_CHECKPOINT_INTERVAL);
  for (uint64_t tick = 0; tick < RUN_TICKS; ++tick) {
    const TickInput input = scripted(tick);
    recorder.record(input, simulation.step(input));
  }
  const auto decoded =
      eng::sim::decodeReplay(eng::sim::encodeReplay(recorder.finish()));
  REQUIRE(decoded.has_value());

  GameWorld fresh(fourPlayers(), twoCharacters());
  Simulation replaying(fresh, TickHashing::ON);
  REQUIRE(eng::sim::verifyReplay(*decoded, replaying).ok());
}
