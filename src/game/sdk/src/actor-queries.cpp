#include <game/sdk/actor-queries.h>

namespace eng::game::sdk {

namespace {

  /// The square of how far @p a is from @p b across the floor.
  float floorDistanceSquared(Vec3 a, Vec3 b) {
    const float dx = a.x - b.x;
    const float dy = a.y - b.y;
    return dx * dx + dy * dy;
  }

}  // namespace

std::optional<LogicActor> findActor(const GameLogicWorld& world,
                                    std::string_view id) {
  for (uint32_t i = 0; i < world.actorCount(); ++i) {
    LogicActor actor = world.actor(i);
    if (actor.id == id) {
      return actor;
    }
  }
  return std::nullopt;
}

std::vector<LogicActor> findActors(const GameLogicWorld& world,
                                   const ActorFilter& filter) {
  std::vector<LogicActor> found;
  for (uint32_t i = 0; i < world.actorCount(); ++i) {
    if (LogicActor actor = world.actor(i); matches(filter, actor)) {
      found.push_back(actor);
    }
  }
  return found;
}

uint32_t countActors(const GameLogicWorld& world, const ActorFilter& filter) {
  uint32_t count = 0;
  for (uint32_t i = 0; i < world.actorCount(); ++i) {
    count += matches(filter, world.actor(i)) ? 1 : 0;
  }
  return count;
}

std::vector<LogicActor> actorsWithin(const GameLogicWorld& world, Vec3 at,
                                     float radius, const ActorFilter& filter) {
  std::vector<LogicActor> found;
  for (uint32_t i = 0; i < world.actorCount(); ++i) {
    LogicActor actor = world.actor(i);
    if (matches(filter, actor) &&
        floorDistanceSquared(actor.position, at) <= radius * radius) {
      found.push_back(actor);
    }
  }
  return found;
}

std::optional<LogicActor> nearestActor(const GameLogicWorld& world, Vec3 at,
                                       const ActorFilter& filter) {
  std::optional<LogicActor> nearest;
  float best = 0.0F;
  for (uint32_t i = 0; i < world.actorCount(); ++i) {
    LogicActor actor = world.actor(i);
    const float d = floorDistanceSquared(actor.position, at);
    if (matches(filter, actor) && (!nearest || d < best)) {
      nearest = actor;
      best = d;
    }
  }
  return nearest;
}

}  // namespace eng::game::sdk
