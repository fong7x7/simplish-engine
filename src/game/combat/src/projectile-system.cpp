#include <algorithm>
#include <engine/physics/segment-queries.h>
#include <engine/sim/slot-move.h>
#include <game/combat/combat-system.h>
#include <span>

namespace eng::game {

namespace {

  /// A fraction of a step no hit is ever at.
  constexpr float NEVER = 2.0F;

  /// The first body a step reaches: how far along it, and which.
  struct BodyHit {
    /// How far along the step, from 0 to 1; `NEVER` for none.
    float at = NEVER;
    /// The body's index in the workspace.
    uint32_t body = 0;
  };

  /// The earliest fraction of @p sweep a box stops it at, or `NEVER`.
  float boxHit(const physics::SegmentSweep& sweep, const CombatScene& scene) {
    const Vec2 mid = (sweep.from + sweep.to) * 0.5F;
    const float reach =
        Vec2::distance(sweep.from, sweep.to) * 0.5F + sweep.radius;
    std::vector<uint32_t>& boxes = scene.workspace.boxes;
    scene.broadphase.gather(mid, reach, boxes);
    float first = NEVER;
    for (const uint32_t index : boxes) {
      if (const auto t = physics::sweepHitsBox(sweep, scene.obstacles[index])) {
        first = std::min(first, *t);
      }
    }
    return first;
  }

  /// The first body opposing @p side that @p sweep reaches, among those the
  /// grid finds near it — the earliest, and of equals the first visited.
  BodyHit bodyHit(const physics::SegmentSweep& sweep, Faction side,
                  const CombatScene& scene) {
    const CombatWorkspace& workspace = scene.workspace;
    const Vec2 mid = (sweep.from + sweep.to) * 0.5F;
    const float reach = Vec2::distance(sweep.from, sweep.to) * 0.5F +
                        sweep.radius + workspace.largest_radius;
    BodyHit hit;
    workspace.grid.forEachNear(mid, reach, [&](uint32_t b) {
      const CombatBody& body = workspace.bodies[b];
      if (!factionsOppose(side, body.side)) {
        return;
      }
      const auto t = physics::sweepHitsCircle(sweep, body.at, body.radius);
      if (t && *t < hit.at) {
        hit = {*t, b};
      }
    });
    return hit;
  }

  /// Mark the projectile at dense index @p i for destruction.
  void land(ProjectilePool& projectiles, uint32_t i) {
    (void)projectiles.slots.destroy(projectiles.slots.handleAt(i));
  }

  /// Move the projectile at dense index @p i one tick, or land it.
  void stepOne(ProjectilePool& projectiles, uint32_t i,
               const CombatScene& scene) {
    const Vec2 from = projectiles.position[i];
    const physics::SegmentSweep sweep{from, from + projectiles.velocity[i],
                                      PROJECTILE_RADIUS_TILES,
                                      PROJECTILE_Z_TILES};
    const float wall = boxHit(sweep, scene);
    const BodyHit body = bodyHit(sweep, projectiles.side[i], scene);
    if (body.at <= std::min(wall, 1.0F)) {
      scene.effects.damage.push_back(
          {scene.workspace.bodies[body.body].who, projectiles.damage[i]});
      land(projectiles, i);
    } else if (wall <= 1.0F || --projectiles.ticks_left[i] == 0) {
      land(projectiles, i);
    } else {
      projectiles.position[i] = sweep.to;
    }
  }

}  // namespace

void stepProjectiles(ProjectilePool& projectiles, const CombatScene& scene) {
  for (uint32_t i = 0; i < projectiles.slots.size(); ++i) {
    if (!projectiles.slots.isPendingDestroy(projectiles.slots.handleAt(i))) {
      stepOne(projectiles, i, scene);
    }
  }
}

void compactProjectiles(ProjectilePool& projectiles) {
  const auto moves = projectiles.slots.compact();
  sim::applySlotMoves(moves, projectiles.position);
  sim::applySlotMoves(moves, projectiles.velocity);
  sim::applySlotMoves(moves, projectiles.ticks_left);
  sim::applySlotMoves(moves, projectiles.damage);
  sim::applySlotMoves(moves, projectiles.side);
}

void hashProjectiles(const ProjectilePool& projectiles,
                     sim::StateHasher& hasher) {
  const uint32_t n = projectiles.slots.size();
  projectiles.slots.hashInto(hasher);
  hasher.addSpan(std::span<const Vec2>(projectiles.position).first(n));
  hasher.addSpan(std::span<const Vec2>(projectiles.velocity).first(n));
  hasher.addSpan(std::span<const uint32_t>(projectiles.ticks_left).first(n));
  hasher.addSpan(std::span<const uint16_t>(projectiles.damage).first(n));
  hasher.addSpan(std::span<const Faction>(projectiles.side).first(n));
}

}  // namespace eng::game
