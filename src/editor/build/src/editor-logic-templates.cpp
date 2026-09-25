#include <editor/build/editor-logic-source.h>

namespace eng::editor {

namespace {

  /// The scaffold's `src/CMakeLists.txt`, as written.
  constexpr std::string_view SCAFFOLD_CMAKE = R"(# This project's own game logic, in C++ (ADR-011 in the engine's docs).
#
# The editor builds this one file two ways:
#   Build > Build Game Logic - a shared library every playtest loads fresh
#   Build > Deploy Game      - linked into the deployed game's executable
#
# List every source file below. SIMPLISH_ROOT is the engine's source tree:
# the editor passes it, and so must you when running CMake by hand:
#   cmake -S src -B build/logic -DSIMPLISH_ROOT=/path/to/simplish

cmake_minimum_required(VERSION 3.25)
project(game-logic LANGUAGES CXX)

include("${SIMPLISH_ROOT}/cmake/SimplishGameLogic.cmake")

simplish_game_logic(
    SOURCES game-logic.cpp
)
)";

  /// The scaffold's example logic, as written.
  constexpr std::string_view SCAFFOLD_SOURCE = R"(// This project's game logic: rules the data tables cannot say.
//
// It runs inside the deterministic tick, once a tick, after damage. Read
// the world and change it through GameLogicWorld only; keep every member a
// later tick decides anything by in hashState; and never read a clock or
// draw a random number but world.random(). The engine's
// docs/game/logic.md says why, and lists everything the world offers.

#include <array>
#include <cstdint>
#include <game/logic/game-logic-entry.h>
#include <game/logic/game-logic.h>
#include <string>

namespace {

/// A tick is 1/60 s.
constexpr uint64_t TICKS_PER_SECOND = 60;
/// Survive this long and the run is won.
constexpr uint64_t SURVIVE_TICKS = 90 * TICKS_PER_SECOND;
/// A wave comes in this often, the first at once.
constexpr uint64_t WAVE_TICKS = 20 * TICKS_PER_SECOND;
/// Players get a segment of health back this often.
constexpr uint64_t REGENERATE_TICKS = 10 * TICKS_PER_SECOND;
/// Where a wave's chasers come in, around player 1, in tiles.
constexpr std::array<eng::Vec3, 4> WAVE_OFFSETS{{
    {9.0F, 0.0F, 0.0F}, {-9.0F, 0.0F, 0.0F},
    {0.0F, 9.0F, 0.0F}, {0.0F, -9.0F, 0.0F}}};

/// Survive 90 seconds against a wave of chasers every 20. Players slowly
/// heal. A project with an enemies table can spawn its own archetypes
/// instead: world.spawnEnemy("grunt", at, "name").
class SurviveTheWaves final : public eng::game::GameLogic {
public:
  void start(eng::game::GameLogicWorld& world) override {
    world.log("Survive " +
              std::to_string(SURVIVE_TICKS / TICKS_PER_SECOND) + " s");
  }

  void tick(eng::game::GameLogicWorld& world) override {
    if (world.outcome() != eng::game::RunOutcome::PLAYING) {
      return;
    }
    if (world.tick() % WAVE_TICKS == 0) {
      sendWave(world);
    }
    if (world.tick() % REGENERATE_TICKS == REGENERATE_TICKS - 1) {
      for (uint32_t i = 0; i < world.playerCount(); ++i) {
        world.heal(world.player(i).target, 1);
      }
    }
    if (world.tick() + 1 >= SURVIVE_TICKS) {
      world.log("Survived " + std::to_string(waves_) + " waves");
      world.endRun(eng::game::RunOutcome::WON);
    }
  }

  void hashState(eng::game::GameLogicHash& hash) const override {
    hash.add(waves_);
  }

private:
  /// Spawn a chaser at each of the wave's places around player 1.
  void sendWave(eng::game::GameLogicWorld& world) {
    const eng::Vec3 centre = world.player(0).position;
    for (const eng::Vec3& offset : WAVE_OFFSETS) {
      (void)world.spawnActor(
          {.at = {centre.x + offset.x, centre.y + offset.y, centre.z},
           .behavior = "chase",
           .id = "wave" + std::to_string(waves_)});
    }
    ++waves_;
    world.log("Wave " + std::to_string(waves_));
  }

  /// Waves sent so far.
  uint32_t waves_ = 0;
};

}  // namespace

SIMPLISH_GAME_LOGIC(SurviveTheWaves)
)";

}  // namespace

std::string logicScaffoldCMake() { return std::string(SCAFFOLD_CMAKE); }

std::string logicScaffoldSource() { return std::string(SCAFFOLD_SOURCE); }

}  // namespace eng::editor
