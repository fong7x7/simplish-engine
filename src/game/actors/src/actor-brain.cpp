#include <algorithm>
#include <engine/core/fixed-step-clock.h>
#include <game/actors/actor-brain.h>

namespace eng::game {

namespace {

  /// The ticks in a second, as a float to divide rates by.
  constexpr float TICKS_PER_SECOND = static_cast<float>(TICK_RATE_HZ);

  /// The cosine of half a view @p degrees wide, or below -1 for a view
  /// that takes in everything.
  float viewCos(float degrees) {
    if (!(degrees < 360.0F)) {
      return -2.0F;
    }
    return math::sinCosDegrees(std::max(degrees, 0.0F) * 0.5F).cos;
  }

}  // namespace

ActorBrain compileBrain(const BehaviorDefinition& behavior) {
  const BehaviorSenses& senses = behavior.senses;
  const float turn =
      behavior.movement.turn_degrees_per_second / TICKS_PER_SECOND;
  return {.behavior = behavior,
          .speed_per_tick = behavior.movement.speed / TICKS_PER_SECOND,
          .turn_per_tick = math::sinCosDegrees(std::clamp(turn, 0.0F, 180.0F)),
          .view_cos = viewCos(senses.view_degrees),
          .sight_squared = senses.sight_range * senses.sight_range,
          .hearing_squared = senses.hearing_range * senses.hearing_range};
}

}  // namespace eng::game
