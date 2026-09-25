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
    TESTS   tests/game-logic-test.cpp
)
)";

  /// The scaffold's example logic, as written.
  constexpr std::string_view SCAFFOLD_SOURCE = R"(// This project's game logic: rules the data tables cannot say.
//
// Written with the Simplish game SDK — <game/sdk/sdk.h> — on top of what
// the engine lets game logic read and change, GameLogicWorld. It runs
// inside the deterministic tick, once a tick, after damage: keep every
// member a later tick decides anything by in onHash, and never read a
// clock or draw a random number but world.random(). The engine's
// docs/game/sdk.md walks through the SDK; docs/game/logic.md says why the
// rules are what they are.

#include <cstdint>
#include <game/sdk/sdk.h>
#include <string>

namespace sdk = eng::game::sdk;
using eng::game::GameLogicHash;
using eng::game::GameLogicWorld;
using eng::game::LogicEvent;
using eng::game::RunOutcome;

namespace {

/// Survive this long and the run is won.
constexpr uint64_t SURVIVE_TICKS = sdk::seconds(90);
/// A wave comes in this often, the first at once.
constexpr sdk::Every WAVES{sdk::seconds(20)};
/// Players get a segment of health back this often.
constexpr sdk::Every REGENERATE{sdk::seconds(10), sdk::seconds(10) - 1};
/// What every player fires: hold the trigger or the left mouse button.
constexpr sdk::Weapon RIFLE{.damage = 1, .refire_ticks = 8};

/// Survive 90 seconds against a wave of chasers every 20, one more each
/// wave, closing in on the arena — where the players started — from all
/// round. Players fire a rifle, and slowly heal.
/// With an enemies table, a wave can be the project's own archetype:
/// set `.enemy = "grunt"` on the ring.
class SurviveTheWaves final : public sdk::Game {
protected:
  void onStart(GameLogicWorld& world) override {
    // Waves come to the arena, not to wherever the players have run: the
    // floor actors can stand on is the level's, and a player who has left
    // it leaves nowhere to spawn round them.
    arena_ = sdk::playersCentre(world);
    world.log("Survive " + std::to_string(SURVIVE_TICKS / sdk::seconds(1)) +
              " s");
  }

  void onTick(GameLogicWorld& world) override {
    if (world.outcome() != RunOutcome::PLAYING) {
      return;
    }
    if (WAVES.due(world.tick())) {
      sendWave(world);
    }
    for (const auto& player : sdk::playersUp(world)) {
      (void)sdk::fireWeapon(world, player, RIFLE, triggers_[player.target]);
    }
    if (REGENERATE.due(world.tick())) {
      for (const auto& player : sdk::playersUp(world)) {
        world.heal(player.target, 1);
      }
    }
    if (world.tick() + 1 >= SURVIVE_TICKS) {
      world.log("Survived " + std::to_string(waves_) + " waves, " +
                std::to_string(kills_) + " kills");
      world.endRun(RunOutcome::WON);
    }
  }

  void onActorDied(GameLogicWorld& world, const LogicEvent& death) override {
    // A kill is the players' when one of them is behind it: a shot, or the
    // blast of something they killed.
    if (sdk::playerBehind(world, death)) {
      ++kills_;
    }
  }

  void onHash(GameLogicHash& hash) const override {
    hash.add(arena_);
    hash.add(waves_);
    hash.add(kills_);
    triggers_.hashInto(hash);
  }

private:
  /// A ring of chasers round the players, one more than the last wave.
  void sendWave(GameLogicWorld& world) {
    ++waves_;
    const uint32_t spawned = sdk::spawnRing(
        world, {.centre = arena_,
                .radius = 7.0F,
                .count = 3 + waves_,
                .actor = {.behavior = "chase", .id = "wave"}});
    world.log("Wave " + std::to_string(waves_) + ": " +
              std::to_string(spawned) + " chasers");
  }

  /// Where the players started: what the waves close in on.
  eng::Vec3 arena_{};
  /// Waves sent so far.
  uint32_t waves_ = 0;
  /// Actors the players have killed so far.
  uint32_t kills_ = 0;
  /// When each player's rifle can fire again.
  sdk::EntityData<sdk::Cooldown> triggers_;
};

}  // namespace

SIMPLISH_GAME_LOGIC(SurviveTheWaves)
)";

  /// The scaffold's example tests, as written.
  constexpr std::string_view SCAFFOLD_TESTS = R"(// Tests of this project's game logic.
//
// Each SIMPLISH_LOGIC_TEST plays a level — by id — with a fresh instance of
// the logic, driven by the test: hold a player's controls, run ticks, read
// the world, expect what should be true. Every Build Game Logic runs them,
// in a process of their own; a failure fails the build, naming this file
// and line. They are built into the playtest library, never into a
// deployed game. The engine's docs/game/sdk.md says more.

#include <cstdint>
#include <game/sdk/sdk.h>

namespace sdk = eng::game::sdk;

SIMPLISH_LOGIC_TEST(first_wave_comes_at_once, "main") {
  test.run(2);
  test.expect(sdk::countActors(test.world(), {.id_prefix = "wave"}) > 0,
              "wave 1 spawned");
  test.expect(test.logged("Wave 1"), "wave 1 announced");
}

SIMPLISH_LOGIC_TEST(the_rifle_kills_what_comes_from_ahead, "main") {
  // Aim at the chaser coming in along +X, and hold the trigger.
  test.hold(0, {.aim_x = 1.0F, .fire = true});
  test.run(2);
  const uint32_t before =
      sdk::countActors(test.world(), {.id_prefix = "wave"});
  test.run(sdk::seconds(4));
  test.expect(sdk::countActors(test.world(), {.id_prefix = "wave"}) < before,
              "a chaser fell to the rifle");
}
)";

}  // namespace

std::string logicScaffoldTests() { return std::string(SCAFFOLD_TESTS); }

std::string logicScaffoldCMake() { return std::string(SCAFFOLD_CMAKE); }

std::string logicScaffoldSource() { return std::string(SCAFFOLD_SOURCE); }

}  // namespace eng::editor
