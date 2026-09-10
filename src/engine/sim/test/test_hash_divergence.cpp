#include <catch2/catch_test_macros.hpp>
#include <engine/sim/hash-divergence.h>
#include <engine/sim/tick-hash-builder.h>

using eng::sim::findDivergence;
using eng::sim::TickHash;
using eng::sim::TickHashBuilder;

namespace {

TickHash hashWith(uint32_t players, uint32_t enemies) {
  TickHashBuilder builder;
  builder.section("players").add(players);
  builder.section("enemies").add(enemies);
  return builder.finish(12);
}

}  // namespace

TEST_CASE("findDivergence finds nothing in equal hashes") {
  CHECK_FALSE(findDivergence(hashWith(1, 2), hashWith(1, 2)).has_value());
}

TEST_CASE("findDivergence names the first section that differs") {
  const auto divergence = findDivergence(hashWith(1, 2), hashWith(1, 3));
  REQUIRE(divergence.has_value());
  CHECK(divergence->tick == 12);
  CHECK(divergence->section == "enemies");
}

TEST_CASE("findDivergence takes names from the live hash") {
  TickHash recorded = hashWith(1, 2);
  for (auto& section : recorded.sections) {
    section.name = {};
  }
  const auto divergence = findDivergence(recorded, hashWith(5, 2));
  REQUIRE(divergence.has_value());
  CHECK(divergence->section == "players");
}

TEST_CASE("findDivergence reports a section-count mismatch unnamed") {
  TickHashBuilder one;
  one.section("players").add(uint32_t{1});
  const auto divergence = findDivergence(one.finish(12), hashWith(1, 2));
  REQUIRE(divergence.has_value());
  CHECK(divergence->section.empty());
}
