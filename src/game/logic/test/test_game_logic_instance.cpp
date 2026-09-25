#include <catch2/catch_test_macros.hpp>
#include <game/logic/game-logic-entry.h>
#include <game/logic/game-logic-instance.h>

using eng::game::GameLogic;
using eng::game::GameLogicFactory;
using eng::game::GameLogicInstance;
using eng::game::GameLogicWorld;

namespace {

/// Instances alive, so a test can see the right destroy ran.
int live_instances = 0;

class Counted final : public GameLogic {
public:
  Counted() { ++live_instances; }
  ~Counted() override { --live_instances; }
  Counted(const Counted&) = delete;
  Counted& operator=(const Counted&) = delete;
  Counted(Counted&&) = delete;
  Counted& operator=(Counted&&) = delete;
  void tick([[maybe_unused]] GameLogicWorld& world) override {}
};

}  // namespace

SIMPLISH_GAME_LOGIC(Counted)

namespace {

GameLogicFactory exported() {
  return {simplishCreateGameLogic, simplishDestroyGameLogic};
}

}  // namespace

TEST_CASE("the exported functions report the headers' API version") {
  REQUIRE(simplishGameLogicApiVersion() == eng::game::GAME_LOGIC_API_VERSION);
}

TEST_CASE("an instance is made by its factory and unmade by it") {
  {
    const GameLogicInstance instance(exported());
    REQUIRE(instance.get() != nullptr);
    REQUIRE(live_instances == 1);
  }
  REQUIRE(live_instances == 0);
}

TEST_CASE("an empty factory makes no instance") {
  const GameLogicInstance instance(GameLogicFactory{});

  REQUIRE(instance.get() == nullptr);
}

TEST_CASE("moving an instance moves who unmakes it") {
  GameLogicInstance first(exported());
  GameLogicInstance second = std::move(first);
  // A moved-from instance holding none is the behaviour under test.
  // NOLINTNEXTLINE(bugprone-use-after-move,clang-analyzer-cplusplus.Move)
  REQUIRE(first.get() == nullptr);
  REQUIRE(live_instances == 1);
  second = GameLogicInstance(exported());
  REQUIRE(live_instances == 1);
}
