#pragma once

/// @file combatant-ref.h
/// @brief A player or an actor, by pool and handle.
/// @par Threading
/// A value type.

#include <engine/sim/entity-handle.h>
#include <game/combat/combatant-kind.h>

namespace eng::game {

/// Something that can be hurt: which pool, and its handle there. A handle
/// rather than a dense index, so it names the same entity whatever
/// compaction does before the damage lands.
struct CombatantRef {
  /// Which pool it is in.
  CombatantKind kind = CombatantKind::PLAYER;
  /// Its handle in that pool.
  sim::EntityHandle handle{};
};

}  // namespace eng::game
