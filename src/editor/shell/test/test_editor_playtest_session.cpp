#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>
#include <cstdint>
#include <editor/shell/editor-player-start-ops.h>
#include <editor/shell/editor-playtest-session.h>
#include <engine/input/player-input-builder.h>
#include <engine/sim/replay-codec.h>
#include <engine/sim/replay-verification.h>
#include <filesystem>
#include <fstream>
#include <game/player/player-system.h>
#include <iterator>
#include <vector>

using Catch::Approx;
using namespace eng;
using namespace eng::editor;

namespace {

/// One 60 Hz frame of real time, as a clock reports it.
constexpr uint64_t FRAME_NS = 16'666'667;

/// Player 1 pushing the stick fully right.
sim::PlayerInput pushingRight() {
  sim::PlayerInput input;
  input.move_x = input::INPUT_AXIS_MAX;
  return input;
}

/// A document whose only start for player 1 is at (4.5, 2.5), behind one
/// for player 2.
EditorDocument documentWithStarts() {
  EditorDocument document;
  document.player_starts.push_back(makeEditorPlayerStart(2, {9.5F, 9.5F, 0}));
  document.player_starts.push_back(makeEditorPlayerStart(1, {4.5F, 2.5F, 0}));
  return document;
}

/// Every byte of the file at @p path.
std::vector<std::byte> readBytes(const std::filesystem::path& path) {
  std::ifstream in(path, std::ios::binary);
  const std::vector<char> chars((std::istreambuf_iterator<char>(in)),
                                std::istreambuf_iterator<char>());
  std::vector<std::byte> bytes;
  for (const char c : chars) {
    bytes.push_back(static_cast<std::byte>(c));
  }
  return bytes;
}

EditorPlaytestSession sessionAt(WorldPoint spawn) {
  EditorDocument document;
  document.player_starts.push_back(makeEditorPlayerStart(1, spawn));
  return {makeEditorPlaytestSetup(document, {}), "main"};
}

}  // namespace

TEST_CASE("a playtest spawns player 1 on the first start for player 1") {
  const game::GameSetup setup =
      makeEditorPlaytestSetup(documentWithStarts(), {0.5F, 0.5F, 0});

  REQUIRE(setup.player_count == 1);
  REQUIRE(setup.seed == EDITOR_PLAYTEST_SEED);
  REQUIRE(setup.spawns[0].x == 4.5F);
  REQUIRE(setup.spawns[0].y == 2.5F);
}

TEST_CASE("a level with no start for player 1 spawns under the camera") {
  EditorDocument document;
  document.player_starts.push_back(makeEditorPlayerStart(3, {9.5F, 9.5F, 0}));

  const game::GameSetup setup =
      makeEditorPlaytestSetup(document, {7.5F, -1.5F, 0});

  REQUIRE(setup.spawns[0].x == 7.5F);
  REQUIRE(setup.spawns[0].y == -1.5F);
}

TEST_CASE("a new playtest is at tick 0 with player 1 at their start") {
  EditorPlaytestSession session = sessionAt({4.5F, 2.5F, 0});
  EditorPlaytestState state;

  session.publish(state);

  REQUIRE(state.tick == 0);
  REQUIRE(state.players.size() == 1);
  REQUIRE(state.players[0].player == 1);
  REQUIRE(state.players[0].position.x == 4.5F);
  REQUIRE_FALSE(state.hash.has_value());
}

TEST_CASE("a step runs one tick on the keyboard's input") {
  EditorPlaytestSession session = sessionAt({0, 0, 0});
  std::vector<EditorScriptedInput> none;
  EditorPlaytestState state;

  session.step(pushingRight(), none);
  session.publish(state);

  REQUIRE(state.tick == 1);
  REQUIRE(state.players[0].position.x ==
          Approx(game::PLAYER_SPEED_TILES_PER_TICK));
  REQUIRE(state.hash.has_value());
}

TEST_CASE("scripted input runs in place of the keyboard until it is used up") {
  EditorPlaytestSession session = sessionAt({0, 0, 0});
  sim::PlayerInput left;
  left.move_x = -input::INPUT_AXIS_MAX;
  std::vector<EditorScriptedInput> scripted = {{left, 2}};

  for (int tick = 0; tick < 3; ++tick) {
    session.step(pushingRight(), scripted);
  }

  // Two ticks left, then the keyboard's one tick right.
  REQUIRE(scripted.empty());
  REQUIRE(session.players().position[0].x ==
          Approx(-game::PLAYER_SPEED_TILES_PER_TICK));
}

TEST_CASE("a second of real time is sixty ticks") {
  EditorPlaytestSession session = sessionAt({0, 0, 0});
  std::vector<EditorScriptedInput> none;
  for (int frame = 0; frame < 60; ++frame) {
    (void)session.advance(FRAME_NS, sim::PlayerInput{}, none);
  }
  REQUIRE(session.tick() == 60);
}

TEST_CASE("a long frame runs four ticks and counts the rest as dropped") {
  EditorPlaytestSession session = sessionAt({0, 0, 0});
  std::vector<EditorScriptedInput> none;
  EditorPlaytestState state;

  const FixedStepAdvance due =
      session.advance(FRAME_NS * 10, sim::PlayerInput{}, none);
  session.publish(state);

  REQUIRE(due.ticks == 4);
  REQUIRE(state.tick == 4);
  REQUIRE(state.dropped_ticks == 6);
}

TEST_CASE("a player is drawn between where the last tick found and left it") {
  EditorPlaytestSession session = sessionAt({0, 0, 0});
  std::vector<EditorScriptedInput> none;
  session.step(pushingRight(), none);

  const float moved = game::PLAYER_SPEED_TILES_PER_TICK;
  REQUIRE(session.renderPosition(0, 0.0F).x == Approx(0.0F));
  REQUIRE(session.renderPosition(0, 0.5F).x == Approx(moved * 0.5F));
  REQUIRE(session.renderPosition(0, 1.0F).x == Approx(moved));
}

TEST_CASE("a playtest's replay reproduces it in a fresh world") {
  EditorDocument document = documentWithStarts();
  const game::GameSetup setup = makeEditorPlaytestSetup(document, {});
  EditorPlaytestSession session(setup, "main");
  std::vector<EditorScriptedInput> none;
  for (uint64_t tick = 0; tick < 200; ++tick) {
    sim::PlayerInput input = pushingRight();
    input.move_y = static_cast<int16_t>(tick % 50 < 25 ? 30000 : -30000);
    session.step(input, none);
  }

  const auto decoded = sim::decodeReplay(sim::encodeReplay(session.replay()));
  REQUIRE(decoded.has_value());
  REQUIRE(decoded->header.level_id == "main");
  game::GameWorld fresh(setup);
  sim::Simulation replaying(fresh, sim::TickHashing::ON);
  REQUIRE(sim::verifyReplay(*decoded, replaying).ok());
}

TEST_CASE("a playtest's replay is written where the project keeps scratch") {
  const auto root =
      std::filesystem::temp_directory_path() / "simplish-playtest-replay-test";
  std::filesystem::remove_all(root);
  EditorPlaytestSession session = sessionAt({0, 0, 0});
  std::vector<EditorScriptedInput> none;
  session.step(pushingRight(), none);

  REQUIRE(writeEditorPlaytestReplay(root, "main", session.replay()));

  const auto path = editorPlaytestReplayPath(root, "main");
  REQUIRE(path == root / "data" / "playtests" / "main.replay");
  const auto read = sim::decodeReplay(readBytes(path));
  REQUIRE(read.has_value());
  REQUIRE(read->inputs.size() == 1);
  std::filesystem::remove_all(root);
}
