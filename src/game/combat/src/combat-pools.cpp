#include <game/combat/combat-effects.h>
#include <game/combat/combat-workspace.h>
#include <game/combat/hazard-pool.h>
#include <game/combat/projectile-pool.h>

namespace eng::game {

ProjectilePool::ProjectilePool(uint32_t capacity)
  : slots(capacity), position(capacity), velocity(capacity),
    ticks_left(capacity), damage(capacity), side(capacity),
    source(capacity, NO_COMBATANT) {}

HazardPool::HazardPool(uint32_t capacity)
  : slots(capacity), position(capacity), radius(capacity), ticks_left(capacity),
    age(capacity), damage(capacity), side(capacity),
    source(capacity, NO_COMBATANT) {}

void clearCombatEffects(CombatEffects& effects) {
  effects.damage.clear();
  effects.blasts.clear();
  effects.shots.clear();
  effects.hazards.clear();
}

CombatWorkspace::CombatWorkspace(uint32_t bodies_capacity,
                                 const spatial::NavGridSpec& floor,
                                 const physics::BoxBroadphase& broadphase)
  : grid(floor, bodies_capacity) {
  bodies.reserve(bodies_capacity);
  points.reserve(bodies_capacity);
  boxes.reserve(broadphase.candidateCapacity());
}

void indexCombatBodies(CombatWorkspace& workspace) {
  workspace.points.clear();
  workspace.largest_radius = 0.0F;
  for (const CombatBody& body : workspace.bodies) {
    workspace.points.push_back(body.at);
    workspace.largest_radius = std::max(workspace.largest_radius, body.radius);
  }
  workspace.grid.rebuild(workspace.points);
}

void hashCombatantRef(const CombatantRef& ref, sim::StateHasher& hasher) {
  hasher.add(ref.kind);
  hasher.add(ref.handle);
}

}  // namespace eng::game
