#include <engine/sim/tick-input.h>
#include <game/actors/actor-workspace.h>

namespace eng::game {

ActorWorkspace::ActorWorkspace(uint32_t actor_capacity,
                               const spatial::NavGrid& grid,
                               const physics::BoxBroadphase& broadphase)
  : finder(grid.cellCount()), intents(actor_capacity),
    neighbors(grid.spec(), actor_capacity) {
  positions.reserve(actor_capacity);
  boxes.reserve(broadphase.candidateCapacity());
  candidates.reserve(actor_capacity + sim::MAX_PLAYERS);
}

}  // namespace eng::game
