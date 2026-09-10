#pragma once

/// @file entity-slots.h
/// @brief Handle bookkeeping for one structure-of-arrays entity pool.
/// @par Threading
/// Main-thread-only; mutated only inside a tick.

#include <cstdint>
#include <engine/sim/entity-handle.h>
#include <engine/sim/slot-move.h>
#include <engine/sim/state-hasher.h>
#include <optional>
#include <span>
#include <vector>

namespace eng::sim {

/// The part of an entity pool that is the same for every archetype: which
/// dense index each handle points at, which slots are free, and what is
/// waiting to be destroyed (ADR-004, Engine REQUIREMENTS §4.2).
///
/// A pool is this plus one array per field, each `capacity()` long and
/// indexed by dense index:
///
/// @code
///   struct RunnerPool {
///     EntitySlots slots{2048};
///     std::vector<Vec2> position = std::vector<Vec2>(2048);
///     std::vector<Vec2> velocity = std::vector<Vec2>(2048);
///   };
/// @endcode
///
/// Systems iterate dense indices `[0, size())` — linear over every field.
///
/// Destruction is deferred. `destroy` only marks; the entity stays live and
/// every dense index stays valid until `compact`, which runs in the
/// `COMPACTION` phase. Compaction fills each hole with the last live entity
/// and returns the moves it made, which the pool applies to its fields with
/// `applySlotMoves`. Holes are filled from the highest dense index down, so
/// the result depends only on what was destroyed — not on the order the
/// destroys were requested in.
///
/// Capacity is fixed at construction: nothing allocates after it, and a full
/// pool refuses to spawn rather than growing mid-tick.
class EntitySlots {
public:
  /// Room for `capacity` live entities.
  explicit EntitySlots(uint32_t capacity);

  /// A new entity at dense index `size()`, or nothing when the pool is full.
  /// Its fields are the pool's to initialise.
  std::optional<EntityHandle> spawn();

  /// Marks `handle`'s entity for destruction at the next `compact`. False
  /// when the handle is stale or the entity is already marked.
  bool destroy(EntityHandle handle);

  /// The dense index of `handle`'s entity, or nothing when it is stale.
  /// An entity marked for destruction still resolves until compaction.
  [[nodiscard]] std::optional<uint32_t> denseIndex(EntityHandle handle) const;

  /// The handle of the entity at `dense_index`, which must be below `size()`.
  [[nodiscard]] EntityHandle handleAt(uint32_t dense_index) const;

  /// True when `handle`'s entity is live and marked for destruction.
  [[nodiscard]] bool isPendingDestroy(EntityHandle handle) const;

  /// Destroys every marked entity and returns the moves compaction made, for
  /// `applySlotMoves`. The span is valid until the next `compact`.
  std::span<const SlotMove> compact();

  /// Folds the bookkeeping into a tick hash: which slots are live and where,
  /// every slot's generation, and the free-slot order that decides the next
  /// handle `spawn` returns.
  void hashInto(StateHasher& hasher) const;

  /// Live entities, including those marked for destruction.
  [[nodiscard]] uint32_t size() const { return size_; }

  /// Most entities the pool holds at once.
  [[nodiscard]] uint32_t capacity() const;

private:
  /// True when `handle` names a live entity in its current generation.
  [[nodiscard]] bool isLive(EntityHandle handle) const;

  /// Removes the entity at `dense_index`, filling its place from the end.
  void removeAt(uint32_t dense_index);

  /// Returns `slot` to the free list under a new generation.
  void retire(uint32_t slot);

  /// Per slot, the generation a handle to its current entity carries.
  std::vector<uint32_t> generations_;
  /// Per slot, its entity's dense index; meaningless while the slot is free.
  std::vector<uint32_t> slot_to_dense_;
  /// Per dense index below `size_`, the slot that entity occupies.
  std::vector<uint32_t> dense_to_slot_;
  /// Free slots, as a stack; `spawn` takes from the back.
  std::vector<uint32_t> free_slots_;
  /// Dense indices marked for destruction since the last compaction.
  std::vector<uint32_t> pending_;
  /// Per slot, 1 while its entity is marked for destruction.
  std::vector<uint8_t> pending_flags_;
  /// The moves the last compaction made.
  std::vector<SlotMove> moves_;
  /// Live entities.
  uint32_t size_ = 0;
};

}  // namespace eng::sim
