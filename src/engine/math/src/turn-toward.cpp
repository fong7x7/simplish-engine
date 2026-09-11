#include <engine/math/turn-toward.h>

namespace eng::math {

Vec2 rotateBy(Vec2 v, SinCos turn) {
  return {v.x * turn.cos - v.y * turn.sin, v.x * turn.sin + v.y * turn.cos};
}

Vec2 turnToward(Vec2 facing, Vec2 desired, SinCos max_turn) {
  if (Vec2::lengthSquared(desired) == 0.0F) {
    return facing;
  }
  const Vec2 want = Vec2::normalize(desired);
  if (Vec2::dot(facing, want) >= max_turn.cos) {
    return want;
  }
  const float side = Vec2::cross(facing, want) < 0.0F ? -1.0F : 1.0F;
  return Vec2::normalize(rotateBy(facing, {side * max_turn.sin, max_turn.cos}));
}

}  // namespace eng::math
