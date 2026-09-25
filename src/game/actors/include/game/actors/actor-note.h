#pragma once

/// @file actor-note.h
/// @brief One thing an actor did in a tick, as it did it.
/// @par Threading
/// A value type.

#include <cstdint>
#include <engine/sim/entity-handle.h>
#include <game/actors/actor-note-kind.h>
#include <game/combat/combatant-ref.h>

namespace eng::game {

/// What the actor passes append to `ActorTickContext::notes` as each thing
/// happens, in the order the passes reached it — so it is the same on
/// every peer. Read and emptied by the world within the tick.
struct ActorNote {
  /// What it did.
  ActorNoteKind kind = ActorNoteKind::STATE_ENTERED;
  /// Which actor.
  sim::EntityHandle actor{};
  /// Whom it noticed, or attacked at; `NO_COMBATANT` for nobody.
  CombatantRef other = NO_COMBATANT;
  /// For `STATE_ENTERED`, the index of the state it entered in its
  /// behavior.
  uint8_t state = 0;
};

}  // namespace eng::game
