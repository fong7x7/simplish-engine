#include <catch2/catch_test_macros.hpp>
#include <cstring>
#include <editor/build/editor-setup-json.h>

using namespace eng::editor;
using eng::game::GameSetup;

namespace {

GameSetup sample() {
  GameSetup setup;
  setup.seed = 0xDEADBEEFCAFEULL;
  setup.player_count = 2;
  setup.actor_capacity = 512;
  setup.spawns[1] = {0.1F, 2.0F / 3.0F, -0.0F};
  setup.characters[0] = "scout";
  setup.obstacles.push_back({{1.0F, 2.0F, 0.0F}, {1.3F, 2.7F, 1.1F}});
  eng::game::ActorSpawn actor;
  actor.id = "boss";
  actor.model = "mesh:boss";
  actor.behavior = "guard";
  actor.faction = eng::game::Faction::FRIENDLY;
  actor.yaw_degrees = 33.3F;
  actor.route = {{1.0F, 1.0F}, {4.5F, 1.0F / 7.0F}};
  actor.health = 9;
  setup.actors.push_back(actor);
  return setup;
}

/// Whether two floats are the same bits: a setup is simulation input.
bool sameBits(float a, float b) {
  return std::memcmp(&a, &b, sizeof(float)) == 0;
}

}  // namespace

TEST_CASE("a setup reads back to the same bits it was written from") {
  const auto read = parseGameSetup(serializeGameSetup(sample()));

  REQUIRE(read.has_value());
  REQUIRE(read->seed == sample().seed);
  REQUIRE(read->player_count == 2);
  REQUIRE(read->actor_capacity == 512);
  REQUIRE(sameBits(read->spawns[1].y, 2.0F / 3.0F));
  REQUIRE(sameBits(read->spawns[1].z, -0.0F));
  REQUIRE(read->characters[0] == "scout");
  REQUIRE(sameBits(read->obstacles[0].max.x, 1.3F));
}

TEST_CASE("a setup's actors read back whole") {
  const auto read = parseGameSetup(serializeGameSetup(sample()));

  REQUIRE(read->actors.size() == 1);
  const eng::game::ActorSpawn& actor = read->actors[0];
  REQUIRE(actor.id == "boss");
  REQUIRE(actor.model == "mesh:boss");
  REQUIRE(actor.behavior == "guard");
  REQUIRE(actor.faction == eng::game::Faction::FRIENDLY);
  REQUIRE(actor.health == 9);
  REQUIRE(sameBits(actor.route[1].y, 1.0F / 7.0F));
  REQUIRE(sameBits(actor.yaw_degrees, 33.3F));
}

TEST_CASE("text that is not a setup reads as nothing") {
  REQUIRE_FALSE(parseGameSetup("not json").has_value());
  REQUIRE_FALSE(parseGameSetup(R"({"schema": "simplish/level/1.0"})"));
  REQUIRE(parseGameSetup(R"({"schema": "simplish/setup/1.0",
                             "seed": "nope", "actors": 3})")
              .has_value());
}
