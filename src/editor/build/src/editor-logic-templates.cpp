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
/// round. Players fire a rifle, and slowly heal. A HUD shows the wave and
/// the kills, and the pause button pauses.
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
    world.showScreen("hud");  // content/ui/hud.ui.json
    sdk::setUiNumber(world, "kills", 0);
  }

  void onTick(GameLogicWorld& world) override {
    if (world.outcome() != RunOutcome::PLAYING) {
      return;
    }
    // Timed by play ticks, which stand still while the game is paused.
    if (WAVES.due(world.playTick())) {
      sendWave(world);
    }
    for (const auto& player : sdk::playersUp(world)) {
      (void)sdk::fireWeapon(world, player, RIFLE, triggers_[player.target]);
    }
    if (REGENERATE.due(world.playTick())) {
      for (const auto& player : sdk::playersUp(world)) {
        world.heal(player.target, 1);
      }
    }
    if (world.playTick() + 1 >= SURVIVE_TICKS) {
      world.log("Survived " + std::to_string(waves_) + " waves, " +
                std::to_string(kills_) + " kills");
      world.endRun(RunOutcome::WON);
    }
  }

  void onActorDied(GameLogicWorld& world, const LogicEvent& death) override {
    // A kill is the players' when one of them is behind it: a shot, or the
    // blast of something they killed.
    if (sdk::playerBehind(world, death)) {
      sdk::setUiNumber(world, "kills", ++kills_);
    }
  }

  // The pause button — P, or a pad's Start — pauses and shows the pause
  // menu (content/ui/pause.ui.json), or plays on. Paused, nothing moves
  // and onTick waits, while the menu and its buttons go on.
  void onPausePressed(GameLogicWorld& world, const LogicEvent&) override {
    (void)sdk::togglePause(world, "pause");
  }

  void onUiAction(GameLogicWorld& world, const LogicEvent& choice) override {
    if (sdk::chose(choice, "resume") && world.paused()) {
      (void)sdk::togglePause(world, "pause");
    } else if (sdk::chose(choice, "give_up")) {
      world.endRun(RunOutcome::LOST);
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
    sdk::setUiNumber(world, "wave", waves_);
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

SIMPLISH_LOGIC_TEST(the_pause_button_stops_the_waves, "main") {
  test.run(2);
  test.hold(0, {.pause = true});
  test.run(2);
  test.hold(0, {});
  test.expect(test.world().paused(), "the pause button paused the game");
  test.expect(test.world().showing("pause"), "and showed the pause menu");
  test.run(sdk::seconds(30));  // past when wave 2 would come
  test.expect(test.uiValue("wave") == "1", "no wave came while paused");
  test.expect(test.choose(0, "resume"), "resume is on the menu");
  test.run(2);
  test.expect(!test.world().paused(), "resume played on");
}
)";

  /// The scaffold's pause menu.
  constexpr std::string_view SCAFFOLD_PAUSE = R"json({
  "schema": "simplish/ui_screen/1.0",
  "layer": "menu",
  "anchor": "center",
  "root": {
    "type": "panel", "width": 320, "padding": [20, 24], "gap": 12,
    "fill": "#181c26f0", "radius": 8, "align": "center",
    "children": [
      {"type": "label", "text": "Paused", "id": "title"},
      {"type": "label", "text": "Wave {wave} - {kills} kills"},
      {"type": "button", "text": "Resume", "action": "resume", "id": "resume",
       "min_width": 200},
      {"type": "button", "text": "Give up", "action": "give_up",
       "id": "give_up", "min_width": 200}
    ]
  }
}
)json";

  /// The scaffold's HUD.
  constexpr std::string_view SCAFFOLD_HUD = R"json({
  "schema": "simplish/ui_screen/1.0",
  "layer": "hud",
  "anchor": "top_left",
  "inset": 12,
  "root": {
    "type": "panel", "direction": "row", "gap": 16, "padding": [6, 12],
    "fill": "#0b0d12b0", "radius": 4,
    "children": [
      {"type": "label", "text": "Wave {wave}", "id": "wave"},
      {"type": "label", "text": "Kills {kills}", "id": "kills"}
    ]
  }
}
)json";

}  // namespace

std::string logicScaffoldPauseScreen() {
  return std::string(SCAFFOLD_PAUSE);
}

std::string logicScaffoldHudScreen() {
  return std::string(SCAFFOLD_HUD);
}

std::string logicScaffoldTests() { return std::string(SCAFFOLD_TESTS); }

std::string logicScaffoldCMake() { return std::string(SCAFFOLD_CMAKE); }

std::string logicScaffoldSource() { return std::string(SCAFFOLD_SOURCE); }

}  // namespace eng::editor
