#include <game/actors/actor-workspace.h>

namespace eng::game {

ActorWorkspace::ActorWorkspace(uint32_t actor_capacity, uint32_t grid_cells)
  : finder(grid_cells), intents(actor_capacity) {}

}  // namespace eng::game
