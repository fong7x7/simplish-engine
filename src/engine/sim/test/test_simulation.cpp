#include <catch2/catch_test_macros.hpp>
#include <engine/sim/simulation.h>
#include <string>
#include <vector>

using eng::sim::Simulation;
using eng::sim::SimulationSystems;
using eng::sim::TickContext;
using eng::sim::TickHashBuilder;
using eng::sim::TickHashing;
using eng::sim::TickInput;
using eng::sim::TickResult;

namespace {

/// Records every phase call, so a test can see the order the tick used.
class PhaseLog final : public SimulationSystems {
public:
  void playerControl(const TickContext& context) override {
    log("player_control", context);
  }
  void enemyAi(const TickContext& context) override {
    log("enemy_ai", context);
  }
  void weaponFire(const TickContext& context) override {
    log("weapon_fire", context);
  }
  void projectiles(const TickContext& context) override {
    log("projectiles", context);
  }
  void damage(const TickContext& context) override { log("damage", context); }
  void director(const TickContext& context) override {
    log("director", context);
  }
  void compaction(const TickContext& context) override {
    log("compaction", context);
  }
  void hashState(TickHashBuilder& builder) const override {
    builder.section("buttons").add(last_buttons);
  }

  /// Every phase call, as "name@tick".
  std::vector<std::string> calls;
  /// Player 0's buttons as the last phase saw them.
  uint32_t last_buttons = 0;

private:
  void log(const std::string& name, const TickContext& context) {
    calls.push_back(name + "@" + std::to_string(context.tick));
    last_buttons = context.input.players[0].buttons;
  }
};

}  // namespace

TEST_CASE("Simulation runs the game's phases in §4.1 order") {
  PhaseLog systems;
  Simulation simulation(systems, TickHashing::OFF);
  (void)simulation.step(TickInput{});
  CHECK(systems.calls ==
        std::vector<std::string>{"player_control@0", "enemy_ai@0",
                                 "weapon_fire@0", "projectiles@0", "damage@0",
                                 "director@0", "compaction@0"});
}

TEST_CASE("Simulation counts ticks from zero") {
  PhaseLog systems;
  Simulation simulation(systems, TickHashing::OFF);
  CHECK(simulation.nextTick() == 0);
  CHECK(simulation.step(TickInput{}).tick == 0);
  CHECK(simulation.step(TickInput{}).tick == 1);
  CHECK(simulation.nextTick() == 2);
}

TEST_CASE("Simulation hands every phase the tick's input") {
  PhaseLog systems;
  Simulation simulation(systems, TickHashing::OFF);
  TickInput input;
  input.players[0].buttons = 5;
  (void)simulation.step(input);
  CHECK(systems.last_buttons == 5);
}

TEST_CASE("Simulation hashes each tick only when hashing is on") {
  PhaseLog off_systems;
  Simulation off(off_systems, TickHashing::OFF);
  CHECK_FALSE(off.step(TickInput{}).hash.has_value());

  PhaseLog on_systems;
  Simulation on(on_systems, TickHashing::ON);
  TickInput input;
  input.players[0].buttons = 9;
  const TickResult result = on.step(input);
  REQUIRE(result.hash.has_value());
  CHECK(result.hash->tick == 0);
  REQUIRE(result.hash->section_count == 1);
  CHECK(result.hash->sections[0].name == "buttons");
}

TEST_CASE("A phase the game does not override does nothing") {
  /// Overrides nothing but the hash.
  class Empty final : public SimulationSystems {
  public:
    void hashState(TickHashBuilder& builder) const override {
      builder.section("empty").add(uint8_t{0});
    }
  };
  Empty systems;
  Simulation simulation(systems, TickHashing::ON);
  const auto first = simulation.step(TickInput{}).hash;
  const auto second = simulation.step(TickInput{}).hash;
  CHECK(first->combined == second->combined);
}
