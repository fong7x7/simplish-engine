#include <catch2/catch_test_macros.hpp>
#include <cstdint>
#include <editor/build/editor-deploy-manifest.h>
#include <editor/build/editor-setup-json.h>
#include <editor/deploy/logic-test-runner.h>
#include <editor/project/project-text-file.h>
#include <filesystem>
#include <game/logic/game-logic.h>
#include <memory>
#include <system_error>

using namespace eng;
using namespace eng::editor;
using game::sdk::LogicTest;
using game::sdk::LogicTestCase;

namespace {

/// Content with one level, `arena`: a player at (1.5, 1.5); and one
/// screen, `over`, with the buttons `retry` and `quit`.
class TestContent {
public:
  TestContent()
    : path_(std::filesystem::temp_directory_path() /
            ("simplish-logic-tests-" +
             // NOLINTNEXTLINE(cppcoreguidelines-pro-type-reinterpret-cast)
             std::to_string(reinterpret_cast<uintptr_t>(this)))) {
    game::GameSetup setup;
    setup.spawns[0] = {1.5F, 1.5F, 0.0F};
    (void)writeProjectTextFile(path_ / "levels" / "arena.setup.json",
                               serializeGameSetup(setup));
    (void)writeProjectTextFile(path_ / "content" / "ui" / "over.ui.json",
                               R"({"root": {"type": "panel", "children": [
             {"type": "button", "text": "Again", "action": "retry"},
             {"type": "button", "text": "Quit", "action": "quit"}]}})");
  }
  ~TestContent() {
    std::error_code ec;
    std::filesystem::remove_all(path_, ec);
  }
  TestContent(const TestContent&) = delete;
  TestContent& operator=(const TestContent&) = delete;
  TestContent(TestContent&&) = delete;
  TestContent& operator=(TestContent&&) = delete;

  /// The folder.
  [[nodiscard]] const std::filesystem::path& path() const { return path_; }

private:
  /// The folder.
  std::filesystem::path path_;
};

/// Logs "tick N" every tick.
class Chatty final : public game::GameLogic {
public:
  void tick(game::GameLogicWorld& world) override {
    world.log("tick " + std::to_string(world.tick()));
  }
};

game::GameLogic* makeChatty() {
  return std::make_unique<Chatty>().release();
}

/// Shows `over` at once; counts each `retry` chosen in the value
/// `tries`, and hides `over` on the first.
class Retrier final : public game::GameLogic {
public:
  void tick(game::GameLogicWorld& world) override {
    if (world.tick() == 0) {
      world.showScreen("over");
    }
    for (const game::LogicEvent& event : world.events()) {
      if (event.kind == game::LogicEventKind::UI_ACTION &&
          event.id == "retry") {
        world.setUiValue("tries", std::to_string(++tries_));
        world.hideScreen("over");
      }
    }
  }

private:
  /// Retries chosen.
  int tries_ = 0;
};

game::GameLogic* makeRetrier() {
  return std::make_unique<Retrier>().release();
}

void unmake(game::GameLogic* logic) {
  const std::unique_ptr<game::GameLogic> owned(logic);
}

void runsAndReads(LogicTest& test) {
  test.run(5);
  test.expect(test.world().tick() == 4, "five ticks ran");
  test.expect(test.logged("tick 3"), "the logic spoke");
}

void walksRight(LogicTest& test) {
  test.hold(0, {.move_x = 1.0F});
  test.run(30);
  test.expect(test.world().player(0).position.x > 2.0F, "the player walked");
}

void fails(LogicTest& test) {
  test.expect(false, "this cannot hold");
}

void choosesRetry(LogicTest& test) {
  test.run(1);
  test.expect(test.world().showing("over"), "the screen is shown");
  test.expect(test.choose(0, "retry"), "retry is an action");
  test.expect(!test.choose(0, "nope"), "nope is not");
  test.run(2);
  test.expect(test.uiValue("tries") == "1", "the choice was heard once");
  test.expect(!test.world().showing("over"), "and the screen hidden");
}

/// @p body as a test of the level `arena`.
LogicTestCase testOf(void (*body)(LogicTest&)) {
  return {"a_test", "arena", 1, body};
}

}  // namespace

TEST_CASE("a logic test runs ticks, reads the world and hears the logic") {
  const TestContent content;

  const LogicTestResult result =
      runLogicTest(testOf(runsAndReads), {makeChatty, unmake}, content.path());

  CHECK(result.passed);
  CHECK(result.ticks == 5);
}

TEST_CASE("a logic test plays a player through what it holds") {
  const TestContent content;

  CHECK(runLogicTest(testOf(walksRight), {}, content.path()).passed);
}

TEST_CASE("a failed expectation fails the test, naming where it was") {
  const TestContent content;

  const LogicTestResult result =
      runLogicTest(testOf(fails), {}, content.path());

  REQUIRE_FALSE(result.passed);
  REQUIRE(result.failures.size() == 1);
  CHECK(result.failures[0].file.ends_with("test_logic_test_runner.cpp"));
  CHECK(result.failures[0].line > 0);
  CHECK(result.failures[0].message == "this cannot hold");
}

TEST_CASE("a logic test of a level that is not there fails, saying so") {
  const TestContent content;
  LogicTestCase test = testOf(runsAndReads);
  test.level = "caves";

  const LogicTestResult result = runLogicTest(test, {}, content.path());

  CHECK_FALSE(result.passed);
  CHECK(result.failures[0].message == "no level caves to play");
}

TEST_CASE("logic test results read back as the editor keeps them") {
  const TestContent content;
  const std::vector<LogicTestResult> results = {
      runLogicTest(testOf(runsAndReads), {makeChatty, unmake}, content.path()),
      runLogicTest(testOf(fails), {}, content.path())};

  const auto read = parseLogicTestResults(logicTestResultsJson(results));

  REQUIRE(read.size() == 2);
  CHECK(read[0].passed);
  CHECK(read[0].ticks == 5);
  CHECK_FALSE(read[1].passed);
  CHECK(read[1].failures[0].line > 0);
}

TEST_CASE("a logic test chooses on a screen, and reads what it shows") {
  const TestContent content;

  const LogicTestResult result =
      runLogicTest(testOf(choosesRetry), {makeRetrier, unmake}, content.path());

  CHECK(result.failures.empty());
  CHECK(result.passed);
}
