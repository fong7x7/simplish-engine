#pragma once

/// @file entity-handle.h
/// @brief A generational reference to one entity in one pool.
/// @par Threading
/// A value type.

#include <cstdint>
#include <type_traits>

namespace eng::sim {

/// A reference to an entity that survives compaction and detects its own
/// staleness (ADR-004). `index` names a slot, which never moves; the slot's
/// dense position may. `generation` changes every time the slot is reused,
/// so a handle kept past its entity's destruction resolves to nothing
/// rather than to whatever was spawned into the slot next.
///
/// Generation 0 is never issued, so a default-constructed handle is null.
struct EntityHandle {
  /// The slot, stable for the entity's whole life.
  uint32_t index = 0;
  /// The slot's generation when this entity was spawned into it.
  uint32_t generation = 0;

  /// Handles are equal when they name the same slot in the same generation.
  bool operator==(const EntityHandle&) const = default;
};

static_assert(std::has_unique_object_representations_v<EntityHandle>,
              "EntityHandle is stored in hashed pool fields");

}  // namespace eng::sim
