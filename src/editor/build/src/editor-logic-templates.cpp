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

#include <cstdint>
#include <game/logic/game-logic-entry.h>
#include <game/logic/game-logic.h>
#include <string>

namespace {

/// A tick is 1/60 s.
constexpr uint64_t TICKS_PER_SECOND = 60;
/// Survive this long and the run is won.
constexpr uint64_t SURVIVE_TICKS = 90 * TICKS_PER_SECOND;
/// Players get a segment of health back this often.
constexpr uint64_t REGENERATE_TICKS = 10 * TICKS_PER_SECOND;

/// Hostile actors still standing.
uint32_t hostilesLeft(const eng::game::GameLogicWorld& world) {
  uint32_t left = 0;
  for (uint32_t i = 0; i < world.actorCount(); ++i) {
    const eng::game::LogicActor actor = world.actor(i);
    if (actor.faction == eng::game::Faction::HOSTILE && actor.health > 0) {
      ++left;
    }
  }
  return left;
}

/// Win by clearing every hostile actor the level starts with, or by
/// surviving long enough. Players slowly heal.
class SurviveOrClear final : public eng::game::GameLogic {
public:
  void start(eng::game::GameLogicWorld& world) override {
    hostiles_ = hostilesLeft(world);
    world.log("Clear " + std::to_string(hostiles_) + " hostiles, or survive " +
              std::to_string(SURVIVE_TICKS / TICKS_PER_SECOND) + " s");
  }

  void tick(eng::game::GameLogicWorld& world) override {
    if (world.outcome() != eng::game::RunOutcome::PLAYING) {
      return;
    }
    if (world.tick() % REGENERATE_TICKS == REGENERATE_TICKS - 1) {
      for (uint32_t i = 0; i < world.playerCount(); ++i) {
        world.heal(world.player(i).target, 1);
      }
    }
    if (hostiles_ > 0 && hostilesLeft(world) == 0) {
      world.log("Cleared");
      world.endRun(eng::game::RunOutcome::WON);
    } else if (world.tick() + 1 >= SURVIVE_TICKS) {
      world.log("Survived");
      world.endRun(eng::game::RunOutcome::WON);
    }
  }

  void hashState(eng::game::GameLogicHash& hash) const override {
    hash.add(hostiles_);
  }

private:
  /// Hostile actors the level started with.
  uint32_t hostiles_ = 0;
};

}  // namespace

SIMPLISH_GAME_LOGIC(SurviveOrClear)
)";

}  // namespace

std::string logicScaffoldCMake() { return std::string(SCAFFOLD_CMAKE); }

std::string logicScaffoldSource() { return std::string(SCAFFOLD_SOURCE); }

}  // namespace eng::editor
