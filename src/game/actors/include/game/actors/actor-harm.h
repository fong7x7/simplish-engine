#pragma once

/// @file actor-harm.h
/// @brief One hit on one actor, as the damage phase hands it over.
/// @par Threading
/// A value type.

#include <cstdint>
#include <game/combat/combatant-ref.h>

namespace eng::game {

/// How much an actor is hurt, and when: what `hurtActor` applies.
struct ActorHarm {
  /// Health segments to take away.
  uint16_t amount = 0;
  /// The tick it happens on, which the actor remembers as when it was
  /// last hurt.
  uint64_t tick = 0;
  /// Who hurt it: credited with its blast, should it die in one.
  CombatantRef source = NO_COMBATANT;
};

}  // namespace eng::game
