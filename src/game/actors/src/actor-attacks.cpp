#include "actor-passes.h"
#include "actor-queries.h"

#include <array>
#include <cstddef>
#include <engine/core/fixed-step-clock.h>
#include <engine/math/sin-cos.h>
#include <game/combat/combat-system.h>
#include <game/player/player-system.h>
#include <limits>
#include <optional>

namespace eng::game {

namespace {

  /// Actor @p a, as combat names who did something.
  CombatantRef self(const ActorRef& a) {
    return {CombatantKind::ACTOR, a.pool.slots.handleAt(a.i)};
  }

  /// One attacking action's strike, shot, pool or blast.
  using AttackFn = void (*)(const ActorRef&, const ActorTickContext&,
                            const BehaviorAttack&);

  /// Whom an actor strikes at: where they stand and how wide they are.
  struct TargetBody {
    /// Who.
    CombatantRef who{};
    /// Where they stand, on the floor.
    Vec2 at{};
    /// Their radius.
    float radius = 0.0F;
  };

  /// Actor @p a's target as it stands now, if they are still in the game
  /// and — a player — up.
  std::optional<TargetBody> targetBody(const ActorRef& a,
                                       const ActorTickContext& context) {
    const sim::EntityHandle handle = a.pool.target[a.i];
    if (a.pool.target_kind[a.i] == CombatantKind::ACTOR) {
      const auto j = a.pool.slots.denseIndex(handle);
      return j ? std::optional{TargetBody{{CombatantKind::ACTOR, handle},
                                          flat(a.pool.position[*j]),
                                          a.pool.radius[*j]}}
               : std::nullopt;
    }
    const auto p = context.players.slots.denseIndex(handle);
    if (!p || !playerIsUp(context.players, *p)) {
      return std::nullopt;
    }
    return TargetBody{{CombatantKind::PLAYER, handle},
                      flat(context.players.position[*p]),
                      PLAYER_RADIUS_TILES};
  }

  /// Whether actor @p a sees its target and its attack's cooldown has run.
  bool canStrike(const ActorRef& a, const ActorTickContext& context) {
    return a.pool.sees_target[a.i] != 0 &&
           context.tick >= a.pool.attack_ready_tick[a.i];
  }

  /// Whom actor @p a has in mind: its target, or nobody.
  CombatantRef targetOf(const ActorRef& a) {
    return a.pool.remembers_target[a.i] != 0
               ? CombatantRef{a.pool.target_kind[a.i], a.pool.target[a.i]}
               : NO_COMBATANT;
  }

  /// Note that actor @p a attacked, at whomever it has in mind.
  void noteAttack(const ActorRef& a, const ActorTickContext& context) {
    context.notes.push_back(
        {ActorNoteKind::ATTACKED, a.pool.slots.handleAt(a.i), targetOf(a)});
  }

  /// Whether actor @p a's attack goes off now: at once, for one with no
  /// wind-up; for one with, once the wind-up it began — and noted — the
  /// first tick it could go off has run.
  bool goesOff(const ActorRef& a, const ActorTickContext& context,
               const BehaviorAttack& attack) {
    uint64_t& lands = a.pool.attack_lands_tick[a.i];
    if (attack.windup_ticks == 0) {
      return true;
    }
    if (lands == ACTOR_NOT_WINDING) {
      lands = context.tick + attack.windup_ticks;
      context.notes.push_back(
          {ActorNoteKind::WINDING_UP, a.pool.slots.handleAt(a.i), targetOf(a)});
      return false;
    }
    if (context.tick < lands) {
      return false;
    }
    lands = ACTOR_NOT_WINDING;
    return true;
  }

  /// Note that actor @p a attacked, and start its attack's cooldown.
  void coolDown(const ActorRef& a, const ActorTickContext& context,
                const BehaviorAttack& attack) {
    noteAttack(a, context);
    a.pool.attack_ready_tick[a.i] = context.tick + attack.cooldown_ticks;
  }

  /// A melee or a charge: hit the target when it is within reach.
  void strike(const ActorRef& a, const ActorTickContext& context,
              const BehaviorAttack& attack) {
    const auto body = targetBody(a, context);
    if (!body || attack.damage == 0 || !canStrike(a, context)) {
      return;
    }
    const float reach = a.pool.radius[a.i] + body->radius + attack.reach_tiles;
    if (Vec2::distanceSquared(flat(a.pool.position[a.i]), body->at) <=
            reach * reach &&
        goesOff(a, context, attack)) {
      context.effects.damage.push_back(
          {body->who, attack.damage, self(a), DamageCause::ATTACK});
      coolDown(a, context, attack);
    }
  }

  /// @p v turned @p degrees counterclockwise.
  Vec2 turned(Vec2 v, float degrees) {
    const math::SinCos turn = math::sinCosDegrees(degrees);
    return {v.x * turn.cos - v.y * turn.sin, v.x * turn.sin + v.y * turn.cos};
  }

  /// Which way actor @p a aims: at where its target was seen, or ahead.
  Vec2 aimOf(const ActorRef& a) {
    const Vec2 toward = a.pool.last_seen[a.i] - flat(a.pool.position[a.i]);
    return Vec2::lengthSquared(toward) > 0.0F ? Vec2::normalize(toward)
                                              : a.pool.facing[a.i];
  }

  /// One shot of actor @p a's, flying @p way from the edge of it.
  ShotRequest shotFrom(const ActorRef& a, Vec2 way,
                       const BehaviorAttack& attack) {
    const float edge = a.pool.radius[a.i] + PROJECTILE_RADIUS_TILES;
    const float step = attack.speed / static_cast<float>(TICK_RATE_HZ);
    return {flat(a.pool.position[a.i]) + way * edge, way * step, attack.damage,
            a.pool.faction[a.i], self(a)};
  }

  /// A volley: `count` shots fanned evenly across the spread, centred on
  /// the target.
  void fire(const ActorRef& a, const ActorTickContext& context,
            const BehaviorAttack& attack) {
    if (attack.count == 0 || !canStrike(a, context) ||
        !goesOff(a, context, attack)) {
      return;
    }
    const Vec2 aim = aimOf(a);
    const bool fans = attack.count > 1;
    const float gap =
        fans ? attack.spread_degrees / static_cast<float>(attack.count - 1)
             : 0.0F;
    const float first = fans ? -attack.spread_degrees * 0.5F : 0.0F;
    for (uint8_t k = 0; k < attack.count; ++k) {
      const float offset = first + static_cast<float>(k) * gap;
      context.effects.shots.push_back(shotFrom(a, turned(aim, offset), attack));
    }
    coolDown(a, context, attack);
  }

  /// A lobbed pool, landing where the target was seen.
  void spit(const ActorRef& a, const ActorTickContext& context,
            const BehaviorAttack& attack) {
    if (!canStrike(a, context) || !goesOff(a, context, attack)) {
      return;
    }
    context.effects.hazards.push_back({a.pool.last_seen[a.i], attack.radius,
                                       attack.damage, attack.duration_ticks,
                                       a.pool.faction[a.i], self(a)});
    coolDown(a, context, attack);
  }

  /// Blow up: the blast is the one it goes off in when it dies, and it
  /// dies.
  void detonate(const ActorRef& a, const ActorTickContext& context,
                const BehaviorAttack& attack) {
    if (!goesOff(a, context, attack)) {
      return;
    }
    a.pool.death_blast_radius[a.i] = attack.radius;
    a.pool.death_blast_damage[a.i] = attack.damage;
    noteAttack(a, context);
    context.effects.damage.push_back({self(a),
                                      std::numeric_limits<uint16_t>::max(),
                                      self(a), DamageCause::ATTACK});
  }

  /// A wind-up of actor @p a's that has run without its attack going off —
  /// its target out of reach or out of sight when it came to land — has
  /// missed: it ends, and the attack cools down as if it had landed.
  void missIfRunOut(const ActorRef& a, const ActorTickContext& context,
                    const BehaviorAttack& attack) {
    uint64_t& lands = a.pool.attack_lands_tick[a.i];
    if (lands != ACTOR_NOT_WINDING && context.tick >= lands) {
      lands = ACTOR_NOT_WINDING;
      a.pool.attack_ready_tick[a.i] = context.tick + attack.cooldown_ticks;
    }
  }

  /// Each action's attack, in enumerator order; null for those that do
  /// not attack.
  constexpr std::array<AttackFn, 15> ATTACKS{
      nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr,
      nullptr, strike,  nullptr, strike,  fire,    spit,    detonate};
  static_assert(ATTACKS.size() ==
                static_cast<size_t>(BehaviorAction::DETONATE) + 1);

}  // namespace

void attackActor(const ActorRef& a, const ActorTickContext& context) {
  const BehaviorState& state = stateOf(a, context);
  const AttackFn attack = ATTACKS[static_cast<size_t>(state.action)];
  if (attack != nullptr) {
    attack(a, context, state.attack);
    missIfRunOut(a, context, state.attack);
  }
}

}  // namespace eng::game
