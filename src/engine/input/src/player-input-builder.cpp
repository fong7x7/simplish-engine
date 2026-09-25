#include <algorithm>
#include <cmath>
#include <engine/input/player-input-builder.h>

namespace eng::input {

namespace {

  /// 1/√2: a diagonal's share of full scale on each axis, so a diagonal is
  /// as fast as a straight line rather than √2 faster.
  constexpr float DIAGONAL_SCALE = 0.70710678F;

  /// The four actions that make one screen-space stick.
  struct StickActions {
    /// Pushes the stick right.
    InputAction right;
    /// Pushes it left.
    InputAction left;
    /// Pushes it down the screen.
    InputAction down;
    /// Pushes it up the screen.
    InputAction up;
  };

  /// The movement stick, and the aim stick.
  constexpr StickActions MOVE_STICK{
      InputAction::MOVE_RIGHT, InputAction::MOVE_LEFT, InputAction::MOVE_DOWN,
      InputAction::MOVE_UP};
  constexpr StickActions AIM_STICK{InputAction::AIM_RIGHT,
                                   InputAction::AIM_LEFT, InputAction::AIM_DOWN,
                                   InputAction::AIM_UP};

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

  /// The screen-space stick @p stick's actions ask for in @p values,
  /// capped at unit length so a diagonal is no faster than a straight line.
  Vec2 valueStick(const ActionValues& values, const StickActions& stick) {
    Vec2 out{values.value(stick.right) - values.value(stick.left),
             values.value(stick.down) - values.value(stick.up)};
    const float length = std::sqrt(out.x * out.x + out.y * out.y);
    if (length > 1.0F) {
      out = {out.x / length, out.y / length};
    }
    return out;
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

  /// The quantised input for a world @p move, an @p aim of any length, and
  /// the `PlayerInput::buttons` bits @p buttons.
  sim::PlayerInput assemble(Vec2 move, Vec2 aim, uint32_t buttons) {
    const Vec2 facing = unitAim(aim);
    sim::PlayerInput input;
    input.move_x = quantizeInputAxis(move.x);
    input.move_y = quantizeInputAxis(move.y);
    input.aim_x = quantizeInputAxis(facing.x);
    input.aim_y = quantizeInputAxis(facing.y);
    input.buttons = buttons;
    return input;
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
  const bool fire = held.held(InputAction::FIRE);
  const bool pause = held.held(InputAction::PAUSE);
  return assemble(move, aim,
                  (fire ? INPUT_BUTTON_FIRE : 0U) |
                      (pause ? INPUT_BUTTON_PAUSE : 0U));
}

sim::PlayerInput makePlayerInput(const ActionValues& values, Vec2 fallback_aim,
                                 const MoveBasis& basis) {
  const Vec2 move = toWorld(valueStick(values, MOVE_STICK), basis);
  const Vec2 stick_aim = valueStick(values, AIM_STICK);
  const bool aiming = stick_aim.x != 0.0F || stick_aim.y != 0.0F;
  const Vec2 aim = aiming ? toWorld(stick_aim, basis) : fallback_aim;
  const bool fire = values.pressed(InputAction::FIRE);
  const bool pause = values.pressed(InputAction::PAUSE);
  return assemble(move, aim,
                  (fire ? INPUT_BUTTON_FIRE : 0U) |
                      (pause ? INPUT_BUTTON_PAUSE : 0U));
}

}  // namespace eng::input
