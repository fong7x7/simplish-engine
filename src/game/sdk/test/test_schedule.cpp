#include "support/list-hash.h"
#include "support/sdk-rig.h"

#include <catch2/catch_test_macros.hpp>
#include <cstdint>
#include <game/sdk/schedule.h>
#include <vector>

using namespace eng::game;
using namespace eng::game::sdk;
using eng::game::sdk::test::ListHash;

TEST_CASE("a schedule hands out each value on its tick, in the order added") {
  Schedule<uint32_t> later;
  std::vector<uint64_t> ran;
  test::ScriptedLogic logic([&](GameLogicWorld& world) {
    if (world.tick() == 0) {
      later.after(world, 3, 30);
      later.after(world, 1, 10);
      later.after(world, 3, 31);
    }
    later.runDue(world, [&](GameLogicWorld& w, uint32_t value) {
      ran.push_back(w.tick() * 100 + value);
    });
  });

  test::runLogic(logic, 5, test::sdkArena(), {});

  CHECK(ran == std::vector<uint64_t>{110, 330, 331});
  CHECK(later.size() == 0);
}

TEST_CASE("a value scheduled for now while running comes out in the same "
          "call") {
  Schedule<uint32_t> later;
  std::vector<uint32_t> ran;
  test::ScriptedLogic logic([&](GameLogicWorld& world) {
    if (world.tick() == 0) {
      later.at(0, 1);
    }
    later.runDue(world, [&](GameLogicWorld& w, uint32_t value) {
      ran.push_back(value);
      if (value == 1) {
        later.at(w.tick(), 2);
      }
    });
  });

  test::runLogic(logic, 1, test::sdkArena(), {});

  CHECK(ran == std::vector<uint32_t>{1, 2});
}

TEST_CASE("a schedule cancels what it is told to, and hashes what it holds") {
  Schedule<uint32_t> later;
  later.at(5, 1);
  later.at(9, 2);
  ListHash both;
  later.hashInto(both);

  later.cancel([](uint32_t value) { return value == 1; });
  ListHash one;
  later.hashInto(one);

  CHECK(later.nextTick() == 9U);
  CHECK(one.added.size() < both.added.size());
}
