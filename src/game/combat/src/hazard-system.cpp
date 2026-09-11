#include <engine/sim/slot-move.h>
#include <game/combat/combat-system.h>
#include <span>

namespace eng::game {

namespace {

  /// Whether @p a and @p b name the same player or actor.
  bool same(const CombatantRef& a, const CombatantRef& b) {
    return a.kind == b.kind && a.handle == b.handle;
  }

  /// Hit everyone @p area reaches — standing in it, their edge within its
  /// radius — that @p catches says to, for its damage each.
  template <typename Catches>
  void hitWithin(const CombatScene& scene, const BlastEvent& area,
                 Catches catches) {
    const CombatWorkspace& workspace = scene.workspace;
    const float query = area.radius + workspace.largest_radius;
    workspace.grid.forEachNear(area.at, query, [&](uint32_t b) {
      const CombatBody& body = workspace.bodies[b];
      const float reach = area.radius + body.radius;
      if (catches(body) &&
          Vec2::distanceSquared(body.at, area.at) <= reach * reach) {
        scene.effects.damage.push_back({body.who, area.damage});
      }
    });
  }

  /// Age the hazard at dense index @p i one tick, biting on its bite ticks.
  void stepOne(HazardPool& hazards, uint32_t i, const CombatScene& scene) {
    if (hazards.age[i] % HAZARD_BITE_INTERVAL_TICKS == 0) {
      const Faction side = hazards.side[i];
      const BlastEvent pool{
          hazards.position[i], hazards.radius[i], hazards.damage[i], {}};
      hitWithin(scene, pool, [side](const CombatBody& body) {
        return factionsOppose(side, body.side);
      });
    }
    ++hazards.age[i];
    if (--hazards.ticks_left[i] == 0) {
      (void)hazards.slots.destroy(hazards.slots.handleAt(i));
    }
  }

}  // namespace

void stepHazards(HazardPool& hazards, const CombatScene& scene) {
  for (uint32_t i = 0; i < hazards.slots.size(); ++i) {
    if (!hazards.slots.isPendingDestroy(hazards.slots.handleAt(i))) {
      stepOne(hazards, i, scene);
    }
  }
}

void resolveBlasts(const CombatScene& scene) {
  // Only hits are appended here; the blasts a death sets off come when the
  // damage phase applies these hits, and it resolves them in turn.
  for (const BlastEvent& blast : scene.effects.blasts) {
    hitWithin(scene, blast, [&blast](const CombatBody& body) {
      return !same(body.who, blast.source);
    });
  }
  scene.effects.blasts.clear();
}

void compactHazards(HazardPool& hazards) {
  const auto moves = hazards.slots.compact();
  sim::applySlotMoves(moves, hazards.position);
  sim::applySlotMoves(moves, hazards.radius);
  sim::applySlotMoves(moves, hazards.ticks_left);
  sim::applySlotMoves(moves, hazards.age);
  sim::applySlotMoves(moves, hazards.damage);
  sim::applySlotMoves(moves, hazards.side);
}

void hashHazards(const HazardPool& hazards, sim::StateHasher& hasher) {
  const uint32_t n = hazards.slots.size();
  hazards.slots.hashInto(hasher);
  hasher.addSpan(std::span<const Vec2>(hazards.position).first(n));
  hasher.addSpan(std::span<const float>(hazards.radius).first(n));
  hasher.addSpan(std::span<const uint32_t>(hazards.ticks_left).first(n));
  hasher.addSpan(std::span<const uint32_t>(hazards.age).first(n));
  hasher.addSpan(std::span<const uint16_t>(hazards.damage).first(n));
  hasher.addSpan(std::span<const Faction>(hazards.side).first(n));
}

}  // namespace eng::game
