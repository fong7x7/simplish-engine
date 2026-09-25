#pragma once

/// @file combatant-ref.h
/// @brief A player or an actor, by pool and handle.
/// @par Threading
/// A value type.

#include <cstdint>
#include <engine/sim/entity-handle.h>
#include <engine/sim/state-hasher.h>
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

/// Nobody: the source of a hit nothing is credited with. A handle no
/// entity ever has — unlike a default `CombatantRef`, which is player 1's
/// first handle.
inline constexpr CombatantRef NO_COMBATANT{CombatantKind::ACTOR,
                                           {UINT32_MAX, UINT32_MAX}};

/// Fold @p ref into @p hasher: its pool and its handle, without the
/// padding between them.
void hashCombatantRef(const CombatantRef& ref, sim::StateHasher& hasher);

}  // namespace eng::game
