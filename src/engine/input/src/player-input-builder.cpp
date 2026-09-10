#include <algorithm>
#include <cmath>
#include <engine/input/player-input-builder.h>

namespace eng::input {

namespace {

  /// 1/√2: a diagonal's share of full scale on each axis, so a diagonal is
  /// as fast as a straight line rather than √2 faster.
  constexpr float DIAGONAL_SCALE = 0.70710678F;

  /// +1 when only @p positive is held, -1 when only @p negative is, and 0
  /// when both or neither are.
  float axisFrom(const HeldActions& held, InputAction positive,
                 InputAction negative) {
    const float up = held.held(positive) ? 1.0F : 0.0F;
    const float down = held.held(negative) ? 1.0F : 0.0F;
    return up - down;
  }

  /// The screen-space stick @p held asks for — X right, Y down the
  /// screen — each in [-1, 1], a diagonal scaled down to the speed of a
  /// straight line.
  Vec2 screenStick(const HeldActions& held) {
    Vec2 move{axisFrom(held, InputAction::MOVE_RIGHT, InputAction::MOVE_LEFT),
              axisFrom(held, InputAction::MOVE_DOWN, InputAction::MOVE_UP)};
    if (move.x != 0.0F && move.y != 0.0F) {
      move.x *= DIAGONAL_SCALE;
      move.y *= DIAGONAL_SCALE;
    }
    return move;
  }

  /// @p stick, in screen directions, as the world direction @p basis says
  /// those point along.
  Vec2 toWorld(Vec2 stick, const MoveBasis& basis) {
    return {stick.x * basis.right.x + stick.y * basis.down.x,
            stick.x * basis.right.y + stick.y * basis.down.y};
  }

  /// @p aim as a unit vector, or zero when it has no length.
  Vec2 unitAim(Vec2 aim) {
    const float length = std::sqrt(aim.x * aim.x + aim.y * aim.y);
    return length > 0.0F ? Vec2{aim.x / length, aim.y / length} : Vec2{};
  }

}  // namespace

int16_t quantizeInputAxis(float value) {
  if (std::isnan(value)) {
    return 0;
  }
  const float scaled =
      std::round(std::clamp(value, -1.0F, 1.0F) * INPUT_AXIS_MAX);
  return static_cast<int16_t>(scaled);
}

sim::PlayerInput makePlayerInput(const HeldActions& held, Vec2 aim,
                                 const MoveBasis& basis) {
  const Vec2 move = toWorld(screenStick(held), basis);
  const Vec2 facing = unitAim(aim);
  sim::PlayerInput input;
  input.move_x = quantizeInputAxis(move.x);
  input.move_y = quantizeInputAxis(move.y);
  input.aim_x = quantizeInputAxis(facing.x);
  input.aim_y = quantizeInputAxis(facing.y);
  input.buttons = held.held(InputAction::FIRE) ? INPUT_BUTTON_FIRE : 0U;
  return input;
}

}  // namespace eng::input
