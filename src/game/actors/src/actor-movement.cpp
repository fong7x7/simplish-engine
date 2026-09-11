#include "actor-passes.h"
#include "actor-queries.h"
#include "actor-tuning.h"

#include <engine/math/turn-toward.h>
#include <engine/physics/cylinder-collision.h>

namespace eng::game {

namespace {

  /// Whether @p v is long enough to count as a movement.
  bool isMovement(Vec2 v) {
    return Vec2::lengthSquared(v) > ACTOR_MIN_STEP_TILES * ACTOR_MIN_STEP_TILES;
  }

  /// Which way actor @p a wants to face, by @p toward; a zero vector when
  /// it has nothing to turn to.
  Vec2 desiredFacing(const ActorRef& a, BehaviorFacing toward,
                     const ActorIntent& intent) {
    const bool remembers = a.pool.remembers_target[a.i] != 0;
    const Vec2 to_target = a.pool.last_seen[a.i] - flat(a.pool.position[a.i]);
    if (toward == BehaviorFacing::TARGET && remembers) {
      return to_target;
    }
    if (intent.moves != 0 && isMovement(intent.moved)) {
      return intent.moved;
    }
    return a.pool.sees_target[a.i] != 0 ? to_target : Vec2{};
  }

  /// Whether @p intent's step was a real one that collision cut to less
  /// than `ACTOR_BLOCKED_FRACTION` of itself.
  bool wasStopped(const ActorIntent& intent) {
    const float wanted = Vec2::lengthSquared(intent.step);
    return isMovement(intent.step) &&
           Vec2::lengthSquared(intent.moved) <
               ACTOR_BLOCKED_FRACTION * ACTOR_BLOCKED_FRACTION * wanted;
  }

}  // namespace

void moveActor(const ActorRef& a, const ActorTickContext& context,
               ActorIntent& intent, std::vector<uint32_t>& boxes) {
  Vec3& at = a.pool.position[a.i];
  const Vec2 before = flat(at);
  const physics::CollisionCylinder body{
      {at.x + intent.step.x, at.y + intent.step.y},
      a.pool.radius[a.i],
      at.z,
      a.pool.height[a.i]};
  context.broadphase.gather(body.center,
                            body.radius + ACTOR_COLLISION_REACH_TILES, boxes);
  const Vec2 clear =
      physics::resolveCylinderAgainstBoxes(body, context.obstacles, boxes);
  at.x = clear.x;
  at.y = clear.y;
  intent.moved = clear - before;
  a.pool.blocked[a.i] = wasStopped(intent) ? 1 : 0;
}

void faceActor(const ActorRef& a, const ActorTickContext& context,
               const ActorIntent& intent) {
  const BehaviorFacing toward = stateOf(a, context).facing;
  if (toward == BehaviorFacing::LOCKED) {
    return;
  }
  a.pool.facing[a.i] =
      math::turnToward(a.pool.facing[a.i], desiredFacing(a, toward, intent),
                       brainOf(a, context).turn_per_tick);
}

}  // namespace eng::game
