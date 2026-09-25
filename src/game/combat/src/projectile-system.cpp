#include <algorithm>
#include <engine/physics/segment-queries.h>
#include <engine/sim/slot-move.h>
#include <game/combat/combat-system.h>
#include <optional>
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

  /// Where a projectile's step ended early, and on what.
  struct Landing {
    /// A body, or a box.
    CombatCueKind kind = CombatCueKind::SHOT_HIT_WALL;
    /// Where the projectile reached it, on the floor.
    Vec2 at{};
    /// The body's index in the workspace, when it was one.
    uint32_t body = 0;
  };

  /// What @p sweep, the step of a projectile on @p side, reaches first: the
  /// first opposing body before any box, then a box, or nothing.
  std::optional<Landing> firstLanding(const physics::SegmentSweep& sweep,
                                      Faction side, const CombatScene& scene) {
    const float wall = boxHit(sweep, scene);
    const BodyHit body = bodyHit(sweep, side, scene);
    const Vec2 step = sweep.to - sweep.from;
    if (body.at <= std::min(wall, 1.0F)) {
      return Landing{CombatCueKind::SHOT_HIT_BODY, sweep.from + step * body.at,
                     body.body};
    }
    if (wall <= 1.0F) {
      return Landing{CombatCueKind::SHOT_HIT_WALL, sweep.from + step * wall};
    }
    return std::nullopt;
  }

  /// Land the projectile at dense index @p i as @p landing says: hurt the
  /// body it reached, if it reached one, and cue where.
  void landAt(ProjectilePool& projectiles, uint32_t i, const CombatScene& scene,
              const Landing& landing) {
    if (landing.kind == CombatCueKind::SHOT_HIT_BODY) {
      scene.effects.damage.push_back({scene.workspace.bodies[landing.body].who,
                                      projectiles.damage[i],
                                      projectiles.source[i]});
    }
    cueCombat(scene.cues, {landing.kind,
                           {landing.at.x, landing.at.y, PROJECTILE_Z_TILES},
                           projectiles.velocity[i],
                           0.0F,
                           projectiles.side[i]});
    land(projectiles, i);
  }

  /// Move the projectile at dense index @p i one tick, or land it.
  void stepOne(ProjectilePool& projectiles, uint32_t i,
               const CombatScene& scene) {
    const Vec2 from = projectiles.position[i];
    const physics::SegmentSweep sweep{from, from + projectiles.velocity[i],
                                      PROJECTILE_RADIUS_TILES,
                                      PROJECTILE_Z_TILES};
    if (const auto landing = firstLanding(sweep, projectiles.side[i], scene)) {
      landAt(projectiles, i, scene, *landing);
    } else if (--projectiles.ticks_left[i] == 0) {
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
  sim::applySlotMoves(moves, projectiles.source);
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
  for (uint32_t i = 0; i < n; ++i) {
    hashCombatantRef(projectiles.source[i], hasher);
  }
}

}  // namespace eng::game
