#pragma once

/// @file actor-brain.h
/// @brief A behavior with its rates worked out per tick.
/// @par Threading
/// A value type; read-only once a run starts.

#include <engine/math/sin-cos.h>
#include <game/content/behavior-definition.h>

namespace eng::game {

/// A behavior as the tick runs it: the definition, plus every rate and
/// angle it needs worked out once, at the start of the run, rather than
/// every tick for every actor running it.
struct ActorBrain {
  /// The behavior itself.
  BehaviorDefinition behavior{};
  /// Tiles moved in one tick at full speed.
  float speed_per_tick = 0.0F;
  /// The most it turns in one tick, as a sine and cosine.
  math::SinCos turn_per_tick{};
  /// The cosine of half its view: a direction within the view has at
  /// least this much of its length along the facing. Below -1 when it sees
  /// all round.
  float view_cos = -2.0F;
  /// Its sight range, squared.
  float sight_squared = 0.0F;
  /// Its hearing range, squared.
  float hearing_squared = 0.0F;
};

/// @p behavior as the tick runs it. Angles go through `sinCosDegrees`, so
/// the result is the same on every machine.
[[nodiscard]] ActorBrain compileBrain(const BehaviorDefinition& behavior);

}  // namespace eng::game
