#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>
#include <cstdint>
#include <editor/shell/editor-actor-placement.h>
#include <editor/shell/editor-player-start-ops.h>
#include <editor/shell/editor-playtest-session.h>
#include <editor/shell/editor-waypoint-ops.h>
#include <engine/input/player-input-builder.h>
#include <engine/sim/replay-codec.h>
#include <engine/sim/replay-verification.h>
#include <filesystem>
#include <fstream>
#include <game/content/character-lookup.h>
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
  return {makeEditorPlaytestSetup(document, {}, {}), {}, "main"};
}

}  // namespace

TEST_CASE("a playtest spawns player 1 on the first start for player 1") {
  const game::GameSetup setup =
      makeEditorPlaytestSetup(documentWithStarts(), {}, {0.5F, 0.5F, 0});

  REQUIRE(setup.player_count == 1);
  REQUIRE(setup.seed == EDITOR_PLAYTEST_SEED);
  REQUIRE(setup.spawns[0].x == 4.5F);
  REQUIRE(setup.spawns[0].y == 2.5F);
}

TEST_CASE("a level with no start for player 1 spawns under the camera") {
  EditorDocument document;
  document.player_starts.push_back(makeEditorPlayerStart(3, {9.5F, 9.5F, 0}));

  const game::GameSetup setup =
      makeEditorPlaytestSetup(document, {}, {7.5F, -1.5F, 0});

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
          Approx(game::characterSpeedPerTick(game::defaultCharacter())));
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
          Approx(-game::characterSpeedPerTick(game::defaultCharacter())));
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

  const float moved = game::characterSpeedPerTick(game::defaultCharacter());
  REQUIRE(session.renderPosition(0, 0.0F).x == Approx(0.0F));
  REQUIRE(session.renderPosition(0, 0.5F).x == Approx(moved * 0.5F));
  REQUIRE(session.renderPosition(0, 1.0F).x == Approx(moved));
}

TEST_CASE("a playtest's replay reproduces it in a fresh world") {
  EditorDocument document = documentWithStarts();
  const game::GameSetup setup = makeEditorPlaytestSetup(document, {}, {});
  EditorPlaytestSession session(setup, {}, "main");
  std::vector<EditorScriptedInput> none;
  for (uint64_t tick = 0; tick < 200; ++tick) {
    sim::PlayerInput input = pushingRight();
    input.move_y = static_cast<int16_t>(tick % 50 < 25 ? 30000 : -30000);
    session.step(input, none);
  }

  const auto decoded = sim::decodeReplay(sim::encodeReplay(session.replay()));
  REQUIRE(decoded.has_value());
  REQUIRE(decoded->header.level_id == "main");
  game::GameWorld fresh(setup, {});
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

TEST_CASE("a playtest collides with the props that collide, and no others") {
  EditorDocument document = documentWithStarts();
  // Asset 0 is missing, so each prop is measured as the unit box on its
  // tile: one at (6, 2), which is solid, and one at (8, 2), which is not.
  EditorPlacement solid;
  solid.position = {6.0F, 2.0F, 0.0F};
  EditorPlacement passable = solid;
  passable.position = {8.0F, 2.0F, 0.0F};
  passable.collides = false;
  document.placements = {solid, passable};

  const game::GameSetup setup = makeEditorPlaytestSetup(document, {}, {});

  REQUIRE(setup.obstacles.size() == 1);
  REQUIRE(setup.obstacles[0].min.x == Approx(6.0F));
  REQUIRE(setup.obstacles[0].max.x == Approx(7.0F));
  REQUIRE(setup.obstacles[0].max.z == Approx(1.0F));
}

TEST_CASE("walking right in a playtest stops at the first solid prop") {
  EditorDocument document = documentWithStarts();
  EditorPlacement crate;
  crate.position = {6.0F, 2.0F, 0.0F};
  document.placements.push_back(crate);
  EditorPlaytestSession session(makeEditorPlaytestSetup(document, {}, {}), {},
                                "main");
  std::vector<EditorScriptedInput> none;

  for (int tick = 0; tick < 120; ++tick) {
    session.step(pushingRight(), none);
  }

  // Player 1 starts at x 4.5 on row 2.5, in line with the crate.
  REQUIRE(session.players().position[0].x ==
          Approx(6.0F - game::PLAYER_RADIUS_TILES));
}

TEST_CASE("each player plays as the character of their first start") {
  EditorDocument document = documentWithStarts();
  document.player_starts[0].character = "character:tank";
  document.player_starts[1].character = "character:scout";
  // A later start for player 1 does not change who they are.
  document.player_starts.push_back(makeEditorPlayerStart(1, {0, 0, 0}));
  document.player_starts.back().character = "character:medic";

  const game::GameSetup setup = makeEditorPlaytestSetup(document, {}, {});

  REQUIRE(setup.characters[0] == "scout");
  REQUIRE(setup.characters[1] == "tank");
  REQUIRE(setup.characters[2].empty());
}

TEST_CASE("a playtest plays and reports the character player 1 picked") {
  EditorDocument document = documentWithStarts();
  game::GameSetup setup = makeEditorPlaytestSetup(document, {}, {});
  setup.characters[0] = "tank";
  game::GameContent content;
  content.characters.push_back({"tank", "Tank", "mesh:tank", 3.0F, 9});
  EditorPlaytestSession session(setup, content, "main");
  std::vector<EditorScriptedInput> none;
  EditorPlaytestState state;

  session.step(pushingRight(), none);
  session.publish(state);

  REQUIRE(session.character(0).model == "mesh:tank");
  REQUIRE(state.players[0].character == "tank");
  REQUIRE(state.players[0].health == 9);
  REQUIRE(session.players().position[0].x == Approx(4.5F + 3.0F / 60.0F));
  REQUIRE(session.replay().header.characters[0] == "tank");
}

TEST_CASE("player 1 plays as their start's character, else the first") {
  const std::vector<game::CharacterDefinition> characters{
      {"scout", "Scout", "", 7.0F, 3}, {"tank", "Tank", "", 3.0F, 9}};
  EditorDocument document = documentWithStarts();
  REQUIRE(editorPlaytestDefaultCharacter(document, characters) == "scout");

  document.player_starts[1].character = "character:tank";
  REQUIRE(editorPlaytestDefaultCharacter(document, characters) == "tank");

  document.player_starts[1].character = "character:gone";
  REQUIRE(editorPlaytestDefaultCharacter(document, characters) == "scout");
  REQUIRE(editorPlaytestDefaultCharacter(document, {}).empty());
}

TEST_CASE("a player is moving on a tick that moved them, and still after") {
  EditorPlaytestSession session = sessionAt({0, 0, 0});
  std::vector<EditorScriptedInput> none;
  REQUIRE(session.gait(0) == EditorCharacterGait::STILL);

  session.step(pushingRight(), none);
  REQUIRE(session.gait(0) == EditorCharacterGait::MOVING);

  session.step(sim::PlayerInput{}, none);
  REQUIRE(session.gait(0) == EditorCharacterGait::STILL);
}

namespace {

/// A document with player 1 at (1.5, 1.5), a crate at (4, 4), and a prop at
/// (8, 1) running @p behavior — or scenery, for none.
EditorDocument documentWithActor(std::string behavior) {
  EditorDocument document;
  document.player_starts.push_back(makeEditorPlayerStart(1, {1.5F, 1.5F, 0}));
  document.placements.push_back({"crate_01", 0, {4.0F, 4.0F, 0.0F}, {}});
  EditorPlacement knight{"knight_01", 0, {8.0F, 1.0F, 0.0F}, {0, 0, 180.0F}};
  knight.behavior = std::move(behavior);
  document.placements.push_back(knight);
  return document;
}

/// What a playtest of @p document reports after @p ticks idle ticks.
EditorPlaytestState publishedAfter(const EditorDocument& document, int ticks) {
  EditorPlaytestSession session(makeEditorPlaytestSetup(document, {}, {}), {},
                                "main");
  session.setActorIds(editorActorIds(document));
  std::vector<EditorScriptedInput> none;
  for (int tick = 0; tick < ticks; ++tick) {
    session.step({}, none);
  }
  EditorPlaytestState state;
  session.publish(state);
  return state;
}

}  // namespace

TEST_CASE("a prop with a behavior plays as an actor, not as a box") {
  const game::GameSetup setup =
      makeEditorPlaytestSetup(documentWithActor("behavior:chase"), {}, {});

  REQUIRE(setup.obstacles.size() == 1);
  REQUIRE(setup.actors.size() == 1);
  REQUIRE(setup.actors[0].behavior == "chase");
  REQUIRE(setup.actors[0].at.x == 8.5F);
  REQUIRE(setup.actors[0].yaw_degrees == 90.0F);
}

TEST_CASE("an actor patrols the points of the route it names, in order") {
  EditorDocument document = documentWithActor("behavior:patrol");
  document.placements[1].route = 2;
  document.waypoints.push_back(makeEditorWaypoint(2, 2, {6.5F, 6.5F, 0}));
  document.waypoints.push_back(makeEditorWaypoint(1, 1, {9.5F, 9.5F, 0}));
  document.waypoints.push_back(makeEditorWaypoint(2, 1, {3.5F, 6.5F, 0}));

  const game::GameSetup setup = makeEditorPlaytestSetup(document, {}, {});

  REQUIRE(setup.actors[0].route.size() == 2);
  REQUIRE(setup.actors[0].route[0].x == 3.5F);
  REQUIRE(setup.actors[0].route[1].x == 6.5F);
}

TEST_CASE("an actor naming no route, or an empty one, has no route") {
  EditorDocument document = documentWithActor("behavior:patrol");
  REQUIRE(makeEditorPlaytestSetup(document, {}, {}).actors[0].route.empty());
  document.placements[1].route = 4;
  REQUIRE(makeEditorPlaytestSetup(document, {}, {}).actors[0].route.empty());
}

TEST_CASE("scenery is a box, and no actor") {
  const game::GameSetup setup =
      makeEditorPlaytestSetup(documentWithActor(""), {}, {});
  REQUIRE(setup.obstacles.size() == 2);
  REQUIRE(setup.actors.empty());
}

TEST_CASE("a playtest reports each actor by the prop it came from") {
  const EditorPlaytestState state =
      publishedAfter(documentWithActor("behavior:chase"), 240);

  REQUIRE(state.actors.size() == 1);
  const EditorPlaytestActor& knight = state.actors[0];
  REQUIRE(knight.id == "knight_01");
  REQUIRE(knight.behavior == "chase");
  REQUIRE(knight.state == "pursue");
  REQUIRE(knight.target == 1);
  // Closing on player 1, who stands at (1.5, 1.5).
  REQUIRE(knight.position.x < 6.0F);
}

TEST_CASE("an actor is drawn between the ticks it moved between") {
  const EditorDocument document = documentWithActor("behavior:chase");
  EditorPlaytestSession session(makeEditorPlaytestSetup(document, {}, {}), {},
                                "main");
  std::vector<EditorScriptedInput> none;
  for (int tick = 0; tick < 10; ++tick) {
    session.step({}, none);
  }
  const auto index = session.actorIndex(0);
  REQUIRE(index.has_value());
  const Vec3 before = session.actorRenderPosition(*index, 0.0F);
  const Vec3 after = session.actorRenderPosition(*index, 1.0F);
  REQUIRE(after.x < before.x);
  REQUIRE(session.actorGait(*index) == EditorCharacterGait::MOVING);
  REQUIRE(session.actorRenderPosition(*index, 0.5F).x ==
          Approx((before.x + after.x) * 0.5F));
  REQUIRE_FALSE(session.actorIndex(1).has_value());
}

TEST_CASE("stand-ins join player 1, each on their own start or beside them") {
  EditorDocument document = documentWithStarts();
  game::GameSetup setup = makeEditorPlaytestSetup(document, {}, {});
  addEditorStandIns(setup, document, 3);

  REQUIRE(setup.player_count == 4);
  // Player 2 has a start of their own; 3 and 4 stand beside player 1.
  REQUIRE(setup.spawns[1].x == document.player_starts[0].position.x);
  REQUIRE(setup.spawns[2].x == setup.spawns[0].x + 3.0F);
}

TEST_CASE("a stand-in plays their player, and says so") {
  EditorDocument document = documentWithStarts();
  game::GameSetup setup = makeEditorPlaytestSetup(document, {}, {});
  addEditorStandIns(setup, document, 1);
  setup.spawns[1] = {setup.spawns[0].x + 8.0F, setup.spawns[0].y, 0.0F};
  EditorPlaytestSession session(setup, {}, "main");
  std::vector<EditorScriptedInput> none;
  for (int tick = 0; tick < 30; ++tick) {
    session.step({}, none);
  }
  EditorPlaytestState state;
  session.publish(state);

  REQUIRE(state.players.size() == 2);
  REQUIRE_FALSE(state.players[0].stand_in);
  REQUIRE(state.players[1].stand_in);
  // It came back toward player 1, who stood still.
  REQUIRE(state.players[1].position.x < setup.spawns[1].x);
  REQUIRE_FALSE(state.run_over);
}
