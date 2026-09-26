#include "support/deployed-content-fixture.h"

#include <catch2/catch_test_macros.hpp>
#include <editor/build/editor-deploy-manifest.h>
#include <editor/build/editor-setup-json.h>
#include <editor/deploy/deployed-game.h>
#include <editor/project/project-text-file.h>
#include <filesystem>
#include <memory>
#include <sstream>
#include <system_error>

using namespace eng;
using namespace eng::editor;
using DeployedContent = eng::editor::test::DeployedContentFixture;

namespace {

/// Loses the run on tick 9, saying so.
class GivesUp final : public game::GameLogic {
public:
  void tick(game::GameLogicWorld& world) override {
    if (world.tick() == 9) {
      world.log("giving up");
      world.endRun(game::RunOutcome::LOST);
    }
  }
};

/// Says how many players the world holds, on tick 0.
class CountsPlayers final : public game::GameLogic {
public:
  void start(game::GameLogicWorld& world) override {
    world.log("players " + std::to_string(world.playerCount()));
  }
  void tick([[maybe_unused]] game::GameLogicWorld& world) override {}
};

/// Says how much room it has to spawn into, on tick 0.
class SaysRoom final : public game::GameLogic {
public:
  void start(game::GameLogicWorld& world) override {
    world.log("room " + std::to_string(world.actorRoom()));
  }
  void tick([[maybe_unused]] game::GameLogicWorld& world) override {}
};

game::GameLogic* makeSaysRoom() {
  return std::make_unique<SaysRoom>().release();
}

game::GameLogic* makeCountsPlayers() {
  return std::make_unique<CountsPlayers>().release();
}

game::GameLogic* makeGivesUp() {
  return std::make_unique<GivesUp>().release();
}

/// Options that play back the replay at @p replay against @p content.
DeployedGameOptions verifying(const DeployedContent& content,
                              const std::filesystem::path& replay) {
  DeployedGameOptions options = content.options(0);
  options.mode = DeployedGameMode::VERIFY;
  options.verify = replay;
  return options;
}

void unmake(game::GameLogic* logic) {
  const std::unique_ptr<game::GameLogic> owned(logic);
}

}  // namespace

TEST_CASE("a deployed game with no logic runs until its ticks run out") {
  const DeployedContent content;
  std::ostringstream out;

  const DeployedGameRun run = runDeployedGame(content.options(30), {}, out);

  CHECK(run.error.empty());
  CHECK(run.level == "arena");
  CHECK(run.players == 1);
  CHECK(run.ticks == 30);
  CHECK(run.outcome == game::RunOutcome::PLAYING);
  CHECK_FALSE(run.logic);
}

TEST_CASE("a deployed game runs its linked logic, which can end the run") {
  const DeployedContent content;
  std::ostringstream out;

  const DeployedGameRun run =
      runDeployedGame(content.options(100), {makeGivesUp, unmake}, out);

  CHECK(run.logic);
  CHECK(run.ticks == 10);
  CHECK(run.outcome == game::RunOutcome::LOST);
  CHECK(out.str() == "[logic 9] giving up\n");
}

TEST_CASE("a deployed game seats as many players as it is asked for") {
  const DeployedContent content;
  std::ostringstream out;
  DeployedGameOptions three = content.options(10);
  three.players = 3;

  CHECK(runDeployedGame(three, {makeCountsPlayers, unmake}, out).players == 3);
  CHECK(out.str() == "[logic 0] players 3\n");
}

TEST_CASE("two runs of a deployed game end on the same hash") {
  const DeployedContent content;
  std::ostringstream out;

  const uint64_t first = runDeployedGame(content.options(120), {}, out).hash;
  const uint64_t second = runDeployedGame(content.options(120), {}, out).hash;

  CHECK(first != 0);
  CHECK(first == second);
}

TEST_CASE("a deployed game that is not there, or lacks the level, says so") {
  const DeployedContent content;
  std::ostringstream out;
  DeployedGameOptions missing = content.options(1);
  missing.level = "caves";

  CHECK_FALSE(runDeployedGame({"/nowhere", "", 1, 1}, {}, out).error.empty());
  CHECK(runDeployedGame(missing, {}, out).error == "No level caves in this "
                                                   "game");
}

TEST_CASE("simplish-game's arguments are read as flag and value pairs") {
  const std::string_view args[] = {"--level", "caves",     "--ticks",
                                   "600",     "--players", "3"};
  const std::string_view bad[] = {"--players", "7"};
  const std::string_view dangling[] = {"--level"};

  const auto options = parseDeployedGameArgs(args);

  REQUIRE(options.has_value());
  CHECK(options->level == "caves");
  CHECK(options->max_ticks == 600);
  CHECK(options->players == 3);
  CHECK_FALSE(parseDeployedGameArgs(bad).has_value());
  CHECK_FALSE(parseDeployedGameArgs(dangling).has_value());
}

TEST_CASE("a deployed game gives its logic room to spawn into") {
  const DeployedContent content;
  std::ostringstream out;

  (void)runDeployedGame(content.options(1), {makeSaysRoom, unmake}, out);

  // The level's one actor is in the room already.
  CHECK(out.str() == "[logic 0] room " +
                         std::to_string(game::GAME_LOGIC_ACTOR_CAPACITY - 1) +
                         "\n");
}

TEST_CASE("a solo run's replay plays back to the same end, and not against "
          "other content") {
  const DeployedContent content;
  const DeployedContent other("changed");
  std::ostringstream out;
  DeployedGameOptions options = content.options(90);
  options.players = 2;
  options.replay = content.path() / "solo.replay";
  const DeployedGameRun played = runDeployedGame(options, {}, out);
  DeployedGameOptions verify = verifying(content, options.replay);

  const DeployedGameRun replayed = runDeployedGame(verify, {}, out);
  verify.content = other.path();

  CHECK(replayed.error.empty());
  CHECK(replayed.ticks == 90);
  CHECK(replayed.hash == played.hash);
  CHECK(runDeployedGame(verify, {}, out).error ==
        "The replay was recorded with other content");
}
