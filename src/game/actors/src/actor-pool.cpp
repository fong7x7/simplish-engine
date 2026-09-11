#include <game/actors/actor-pool.h>

namespace eng::game {

ActorPool::ActorPool(uint32_t capacity)
  : slots(capacity), position(capacity), facing(capacity), home(capacity),
    radius(capacity), height(capacity), brain(capacity), faction(capacity),
    state(capacity), state_since(capacity), target(capacity),
    sees_target(capacity), hears_target(capacity), remembers_target(capacity),
    last_seen(capacity), last_seen_tick(capacity), goal(capacity),
    has_goal(capacity), arrived(capacity), blocked(capacity), no_path(capacity),
    path(capacity) {}

}  // namespace eng::game
