#pragma once

/// @file logic-slot-notes.h
/// @brief What a world keeps per slot that its game logic's events name.
/// @par Threading
/// A view over a world's own lists, for one call.

#include <game/combat/combatant-ref.h>
#include <span>
#include <string>

namespace eng::game {

/// A `GameWorld`'s per-slot notes, as `LogicEventLog` reads them at the end
/// of a tick: each indexed by the slot of the entity's handle, so it names
/// the same entity however compaction moves the pools.
struct LogicSlotNotes {
  /// Each actor's name.
  std::span<const std::string> actor_ids;
  /// Who last hurt each actor.
  std::span<const CombatantRef> actor_hurt_by;
  /// Who last hurt each player.
  std::span<const CombatantRef> player_hurt_by;
};

}  // namespace eng::game
