#include <game/sdk/actor-filter.h>

namespace eng::game::sdk {

bool matches(const ActorFilter& filter, const LogicActor& actor) {
  return (filter.life == ActorLife::ANY || actor.health > 0) &&
         (!filter.faction || actor.faction == *filter.faction) &&
         actor.id.starts_with(filter.id_prefix);
}

}  // namespace eng::game::sdk
