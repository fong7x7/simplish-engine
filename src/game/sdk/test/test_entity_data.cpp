#include "support/list-hash.h"
#include "support/sdk-rig.h"

#include <catch2/catch_test_macros.hpp>
#include <cstdint>
#include <game/sdk/entity-data.h>
#include <vector>

using namespace eng::game;
using namespace eng::game::sdk;

using eng::game::sdk::test::ListHash;

TEST_CASE("entity data keeps a value per target, in target order") {
  EntityData<uint32_t> data;
  const LogicTarget b{LogicTargetKind::ACTOR, 2, 0};
  const LogicTarget a{LogicTargetKind::ACTOR, 1, 0};

  data[b] = 20;
  data[a] = 10;

  REQUIRE(data.size() == 2);
  CHECK(data.begin()->first == a);
  CHECK(*data.find(b) == 20);
  data.erase(a);
  CHECK(data.find(a) == nullptr);
}

TEST_CASE("entity data hashes the same whatever order it was filled in") {
  EntityData<uint32_t> first;
  EntityData<uint32_t> second;
  first[{LogicTargetKind::ACTOR, 1, 0}] = 1;
  first[{LogicTargetKind::PLAYER, 0, 0}] = 2;
  second[{LogicTargetKind::PLAYER, 0, 0}] = 2;
  second[{LogicTargetKind::ACTOR, 1, 0}] = 1;
  ListHash a;
  ListHash b;

  first.hashInto(a);
  second.hashInto(b);

  CHECK(a.added == b.added);
}

TEST_CASE("entity data forgets the entities that are gone") {
  EntityData<uint32_t> data;
  test::ScriptedLogic logic([&](GameLogicWorld& world) {
    if (world.tick() == 0) {
      data[world.actor(0).target] = 1;
      data[{LogicTargetKind::ACTOR, 30, 7}] = 2;
      data.forgetGone(world);
    }
  });

  test::runLogic(logic, 1, test::sdkArena(), {});

  CHECK(data.size() == 1);
}
