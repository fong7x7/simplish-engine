#include <catch2/catch_test_macros.hpp>
#include <engine/sim/tick-hash-builder.h>

using eng::sim::TickHash;
using eng::sim::TickHashBuilder;

TEST_CASE("TickHashBuilder keeps sections in the order they were started") {
  TickHashBuilder builder;
  builder.section("players").add(uint32_t{1});
  builder.section("enemies").add(uint32_t{2});
  const TickHash hash = builder.finish(7);
  CHECK(hash.tick == 7);
  REQUIRE(hash.section_count == 2);
  CHECK(hash.sections[0].name == "players");
  CHECK(hash.sections[1].name == "enemies");
  CHECK(hash.sections[0].hash != hash.sections[1].hash);
}

TEST_CASE("TickHashBuilder's combined hash depends on section order") {
  TickHashBuilder forward;
  forward.section("a").add(uint32_t{1});
  forward.section("b").add(uint32_t{2});
  TickHashBuilder backward;
  backward.section("b").add(uint32_t{2});
  backward.section("a").add(uint32_t{1});
  CHECK(forward.finish(0).combined != backward.finish(0).combined);
}

TEST_CASE("TickHashBuilder with no sections gives an empty hash") {
  const TickHash hash = TickHashBuilder{}.finish(3);
  CHECK(hash.section_count == 0);
  CHECK(hash.activeSections().empty());
}

TEST_CASE("Equal state builds an equal tick hash") {
  const auto build = [] {
    TickHashBuilder builder;
    builder.section("state").add(uint64_t{99});
    return builder.finish(1);
  };
  CHECK(build().combined == build().combined);
}
