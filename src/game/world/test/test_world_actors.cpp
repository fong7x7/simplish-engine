#include <catch2/catch_test_macros.hpp>
#include <engine/sim/simulation.h>
#include <game/content/behavior-lookup.h>
#include <game/world/game-world.h>

using eng::game::ActorSpawn;
using eng::game::Faction;
using eng::game::GameContent;
using eng::game::GameSetup;
using eng::game::GameWorld;
using eng::sim::Simulation;
using eng::sim::TickHashing;
using eng::sim::TickInput;

namespace {

/// One player at (1.5, 1.5), a crate, and actors running @p behaviors in a
/// row along y = 6.
GameSetup withActors(std::initializer_list<const char*> behaviors) {
  GameSetup setup;
  setup.spawns[0] = {1.5F, 1.5F, 0.0F};
  setup.obstacles.push_back({{3.0F, 3.0F, 0.0F}, {4.0F, 4.0F, 1.0F}});
  float x = 0.0F;
  for (const char* behavior : behaviors) {
    setup.actors.push_back({.at = {x, 6.0F, 0.0F}, .behavior = behavior});
    x += 2.0F;
  }
  return setup;
}

}  // namespace

TEST_CASE("a world spawns the setup's actors in order, handle for handle") {
  const GameWorld world(withActors({"guard", "chase", "guard"}), {});

  REQUIRE(world.actors().slots.size() == 3);
  REQUIRE(world.actorHandles().size() == 3);
  const auto third = world.actors().slots.denseIndex(world.actorHandles()[2]);
  REQUIRE(third == 2U);
  REQUIRE(world.actors().position[2].x == 4.0F);
}

TEST_CASE("actors running one behavior share one brain") {
  const GameWorld world(withActors({"guard", "chase", "guard"}), {});
  REQUIRE(world.brains().size() == 2);
  REQUIRE(world.actors().brain[0] == world.actors().brain[2]);
  REQUIRE(world.brains()[world.actors().brain[1]].behavior.id == "chase");
}

TEST_CASE("an actor naming a behavior nobody has runs idle") {
  const GameWorld world(withActors({"nonsense"}), {});
  REQUIRE(world.brains()[world.actors().brain[0]].behavior.id == "idle");
}

TEST_CASE("a project's behavior replaces the built-in of the same id") {
  GameContent content;
  content.behaviors.push_back(eng::game::resolveBehavior({}, "idle"));
  content.behaviors.back().id = "chase";
  content.behaviors.back().name = "Lazy Chase";
  const GameWorld world(withActors({"chase"}), content);
  REQUIRE(world.brains()[0].behavior.name == "Lazy Chase");
}

TEST_CASE("the navigation grid is built only when there are actors") {
  REQUIRE(GameWorld(withActors({}), {}).navGrid().cellCount() == 0);
  const GameWorld world(withActors({"idle"}), {});
  REQUIRE(world.navGrid().cellCount() > 0);
  REQUIRE(world.navGrid().clearance(*world.navGrid().cellAt({3.5F, 3.5F})) ==
          0);
}

TEST_CASE("a chasing actor in a world closes on the player") {
  GameWorld world(withActors({"chase"}), {});
  Simulation simulation(world, TickHashing::OFF);
  for (int tick = 0; tick < 300; ++tick) {
    (void)simulation.step(TickInput{});
  }
  const auto& at = world.actors().position[0];
  const float dx = at.x - 1.5F;
  const float dy = at.y - 1.5F;
  REQUIRE(dx * dx + dy * dy < 1.0F);
}

TEST_CASE("an actor's faction comes from its spawn") {
  GameSetup setup = withActors({"follower"});
  setup.actors[0].faction = Faction::FRIENDLY;
  const GameWorld world(setup, {});
  REQUIRE(world.actors().faction[0] == Faction::FRIENDLY);
}

TEST_CASE("an actor spawned with a route patrols it; one without has none") {
  GameSetup setup = withActors({"patrol", "patrol"});
  setup.actors[0].route = {{0.0F, 6.0F}, {0.0F, 9.0F}};
  const GameWorld world(setup, {});
  REQUIRE(world.actors().route[0] == 0);
  REQUIRE(world.actors().route[1] == eng::game::ACTOR_NO_ROUTE);
}
