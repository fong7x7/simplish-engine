#include <array>
#include <catch2/catch_test_macros.hpp>
#include <engine/sim/entity-slots.h>
#include <engine/sim/slot-move.h>
#include <vector>

using eng::sim::applySlotMoves;
using eng::sim::EntityHandle;
using eng::sim::EntitySlots;
using eng::sim::StateHasher;

namespace {

/// Spawns `count` entities and returns their handles, in spawn order.
std::vector<EntityHandle> spawnMany(EntitySlots& slots, int count) {
  std::vector<EntityHandle> handles;
  for (int i = 0; i < count; ++i) {
    handles.push_back(*slots.spawn());
  }
  return handles;
}

uint64_t hashOf(const EntitySlots& slots) {
  StateHasher hasher;
  slots.hashInto(hasher);
  return hasher.value();
}

}  // namespace

TEST_CASE("EntitySlots spawns into dense order from slot 0") {
  EntitySlots slots(4);
  const auto handles = spawnMany(slots, 3);
  CHECK(slots.size() == 3);
  for (uint32_t i = 0; i < 3; ++i) {
    CHECK(handles[i].index == i);
    CHECK(slots.denseIndex(handles[i]) == i);
    CHECK(slots.handleAt(i) == handles[i]);
  }
}

TEST_CASE("EntitySlots refuses to spawn past capacity") {
  EntitySlots slots(2);
  (void)spawnMany(slots, 2);
  CHECK_FALSE(slots.spawn().has_value());
  CHECK(slots.size() == 2);
}

TEST_CASE("A default EntityHandle never resolves") {
  EntitySlots slots(4);
  (void)spawnMany(slots, 4);
  CHECK_FALSE(slots.denseIndex(EntityHandle{}).has_value());
  CHECK_FALSE(slots.destroy(EntityHandle{}));
}

TEST_CASE("A handle to a slot that was never spawned does not resolve") {
  EntitySlots slots(4);
  (void)slots.spawn();
  CHECK_FALSE(slots.denseIndex(EntityHandle{2, 1}).has_value());
  CHECK_FALSE(slots.denseIndex(EntityHandle{99, 1}).has_value());
}

TEST_CASE("EntitySlots::destroy is deferred until compact") {
  EntitySlots slots(4);
  const auto handles = spawnMany(slots, 3);
  REQUIRE(slots.destroy(handles[1]));
  CHECK(slots.size() == 3);
  CHECK(slots.denseIndex(handles[1]) == 1U);
  CHECK(slots.isPendingDestroy(handles[1]));

  (void)slots.compact();
  CHECK(slots.size() == 2);
  CHECK_FALSE(slots.denseIndex(handles[1]).has_value());
  CHECK_FALSE(slots.isPendingDestroy(handles[1]));
}

TEST_CASE("Destroying twice in one tick marks once") {
  EntitySlots slots(4);
  const auto handles = spawnMany(slots, 2);
  CHECK(slots.destroy(handles[0]));
  CHECK_FALSE(slots.destroy(handles[0]));
  (void)slots.compact();
  CHECK(slots.size() == 1);
}

TEST_CASE("Compaction fills a hole with the last entity and reports it") {
  EntitySlots slots(4);
  const auto handles = spawnMany(slots, 4);
  REQUIRE(slots.destroy(handles[1]));
  const auto moves = slots.compact();
  REQUIRE(moves.size() == 1);
  CHECK(moves[0].from == 3);
  CHECK(moves[0].to == 1);
  CHECK(slots.denseIndex(handles[3]) == 1U);
  CHECK(slots.denseIndex(handles[0]) == 0U);
  CHECK(slots.denseIndex(handles[2]) == 2U);
}

TEST_CASE("Destroying the last entity moves nothing") {
  EntitySlots slots(4);
  const auto handles = spawnMany(slots, 3);
  REQUIRE(slots.destroy(handles[2]));
  CHECK(slots.compact().empty());
  CHECK(slots.size() == 2);
}

TEST_CASE("Destroying every entity empties the pool") {
  EntitySlots slots(4);
  for (const EntityHandle handle : spawnMany(slots, 4)) {
    REQUIRE(slots.destroy(handle));
  }
  CHECK(slots.compact().empty());
  CHECK(slots.size() == 0);
  CHECK(slots.spawn().has_value());
}

TEST_CASE("Compaction does not depend on the order destroys were requested") {
  EntitySlots forward(8);
  EntitySlots backward(8);
  const auto a = spawnMany(forward, 8);
  const auto b = spawnMany(backward, 8);
  for (const int i : {1, 4, 6}) {
    REQUIRE(forward.destroy(a[static_cast<std::size_t>(i)]));
  }
  for (const int i : {6, 4, 1}) {
    REQUIRE(backward.destroy(b[static_cast<std::size_t>(i)]));
  }
  const auto forward_moves = forward.compact();
  const std::vector<eng::sim::SlotMove> kept(forward_moves.begin(),
                                             forward_moves.end());
  const auto backward_moves = backward.compact();
  REQUIRE(kept.size() == backward_moves.size());
  CHECK(hashOf(forward) == hashOf(backward));
}

TEST_CASE("A reused slot gets a new generation and the old handle is stale") {
  EntitySlots slots(1);
  const EntityHandle first = *slots.spawn();
  REQUIRE(slots.destroy(first));
  (void)slots.compact();
  const EntityHandle second = *slots.spawn();
  CHECK(second.index == first.index);
  CHECK(second.generation != first.generation);
  CHECK_FALSE(slots.denseIndex(first).has_value());
  CHECK(slots.denseIndex(second) == 0U);
}

TEST_CASE("applySlotMoves keeps every field with its entity") {
  EntitySlots slots(5);
  std::array<int, 5> tag{};
  std::vector<EntityHandle> handles;
  for (int i = 0; i < 5; ++i) {
    const EntityHandle handle = *slots.spawn();
    tag[*slots.denseIndex(handle)] = 100 + i;
    handles.push_back(handle);
  }
  REQUIRE(slots.destroy(handles[0]));
  REQUIRE(slots.destroy(handles[2]));
  applySlotMoves(slots.compact(), tag);
  for (const int i : {1, 3, 4}) {
    const auto index = slots.denseIndex(handles[static_cast<std::size_t>(i)]);
    REQUIRE(index.has_value());
    CHECK(tag[*index] == 100 + i);
  }
}

TEST_CASE("EntitySlots hashes differently once its layout differs") {
  EntitySlots a(4);
  EntitySlots b(4);
  const auto handles = spawnMany(a, 2);
  (void)spawnMany(b, 2);
  CHECK(hashOf(a) == hashOf(b));
  REQUIRE(a.destroy(handles[0]));
  (void)a.compact();
  CHECK(hashOf(a) != hashOf(b));
}
