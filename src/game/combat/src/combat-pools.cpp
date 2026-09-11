#include <game/combat/combat-effects.h>
#include <game/combat/combat-workspace.h>
#include <game/combat/hazard-pool.h>
#include <game/combat/projectile-pool.h>

namespace eng::game {

ProjectilePool::ProjectilePool(uint32_t capacity)
  : slots(capacity), position(capacity), velocity(capacity),
    ticks_left(capacity), damage(capacity), side(capacity) {}

HazardPool::HazardPool(uint32_t capacity)
  : slots(capacity), position(capacity), radius(capacity), ticks_left(capacity),
    age(capacity), damage(capacity), side(capacity) {}

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

}  // namespace eng::game
