#include <catch2/catch_test_macros.hpp>
#include <cstdint>
#include <engine/sim/simulation.h>
#include <functional>
#include <game/logic/game-logic.h>
#include <game/world/game-world.h>
#include <string>
#include <utility>
#include <vector>

using eng::game::GameContent;
using eng::game::GameLogic;
using eng::game::GameLogicWorld;
using eng::game::GameSetup;
using eng::game::GameWorld;
using eng::game::LogicEvent;
using eng::game::LogicEventKind;
using eng::sim::Simulation;
using eng::sim::TickHashing;
using eng::sim::TickInput;

namespace {

/// A logic whose tick is whatever a test says, keeping the choices it
/// hears.
class Menus final : public GameLogic {
public:
  explicit Menus(std::function<void(GameLogicWorld&)> each)
    : each_(std::move(each)) {}
  void tick(GameLogicWorld& world) override {
    for (const LogicEvent& event : world.events()) {
      if (event.kind == LogicEventKind::UI_ACTION) {
        chosen_.emplace_back(event.id);
      }
    }
    each_(world);
  }

  /// Every action chosen, in order.
  std::vector<std::string> chosen_;

private:
  /// The test's tick.
  std::function<void(GameLogicWorld&)> each_;
};

/// A project with two screens, and the actions their buttons name.
GameContent withScreens() {
  GameContent content;
  content.ui_screens = {"hud", "pause"};
  content.ui_actions = {"quit", "resume"};
  return content;
}

/// The combined hash of tick 3 in a world whose logic does @p each.
uint64_t hashAfter(const std::function<void(GameLogicWorld&)>& each) {
  Menus logic(each);
  GameWorld world(GameSetup{}, withScreens(), &logic);
  Simulation simulation(world, TickHashing::ON);
  for (int i = 0; i < 3; ++i) {
    (void)simulation.step(TickInput{});
  }
  return simulation.step(TickInput{}).hash.value().combined;
}

}  // namespace

TEST_CASE("game logic shows and hides screens, and sets what they show") {
  Menus logic([](GameLogicWorld& world) {
    if (world.tick() == 0) {
      world.showScreen("pause");
      world.showScreen("hud");
      world.setUiValue("score", "12");
      world.showScreen("shop");
    } else if (world.tick() == 1) {
      world.hideScreen("pause");
    }
  });
  GameWorld world(GameSetup{}, withScreens(), &logic);
  Simulation simulation(world, TickHashing::ON);

  (void)simulation.step(TickInput{});
  CHECK(world.ui().open == std::vector<std::string>{"pause", "hud"});
  CHECK(world.ui().values.at("score") == "12");
  CHECK(world.takeLogicLog()[0].find("no screen called 'shop'") !=
        std::string::npos);
  (void)simulation.step(TickInput{});
  CHECK(world.ui().open == std::vector<std::string>{"hud"});
}

TEST_CASE("screens shown change nothing a tick hashes") {
  const auto quiet = [](GameLogicWorld&) {
  };
  const auto shown = [](GameLogicWorld& world) {
    world.showScreen("pause");
    world.setUiValue("score", std::to_string(world.tick()));
  };

  CHECK(hashAfter(quiet) == hashAfter(shown));
}

TEST_CASE("a choice in a player's input is heard by the logic, by name") {
  Menus logic([](GameLogicWorld&) {});
  GameWorld world(GameSetup{}, withScreens(), &logic);
  Simulation simulation(world, TickHashing::ON);
  TickInput resume;
  resume.players[0].ui_action = 2;
  TickInput unknown;
  unknown.players[0].ui_action = 9;

  (void)simulation.step(resume);
  (void)simulation.step(unknown);
  (void)simulation.step(TickInput{});

  CHECK(logic.chosen_ == std::vector<std::string>{"resume"});
}
