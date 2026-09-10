#include <algorithm>
#include <engine/core/assert.h>
#include <engine/sim/entity-slots.h>
#include <functional>

namespace eng::sim {

namespace {

  /// Marks a free slot's dense index: never a valid one.
  constexpr uint32_t NO_DENSE_INDEX = UINT32_MAX;

  /// The generation after `generation`, skipping 0 so no handle is ever null
  /// by accident — even after a slot has been reused four billion times.
  uint32_t nextGeneration(uint32_t generation) {
    const uint32_t next = generation + 1U;
    return next == 0U ? 1U : next;
  }

}  // namespace

EntitySlots::EntitySlots(uint32_t capacity)
  : generations_(capacity, 1U), slot_to_dense_(capacity, NO_DENSE_INDEX),
    dense_to_slot_(capacity, 0U), pending_flags_(capacity, 0U) {
  free_slots_.reserve(capacity);
  pending_.reserve(capacity);
  moves_.reserve(capacity);
  // Pushed highest first, so spawning takes slot 0, then 1, and so on.
  for (uint32_t slot = capacity; slot > 0; --slot) {
    free_slots_.push_back(slot - 1U);
  }
}

std::optional<EntityHandle> EntitySlots::spawn() {
  if (free_slots_.empty()) {
    return std::nullopt;
  }
  const uint32_t slot = free_slots_.back();
  free_slots_.pop_back();
  slot_to_dense_[slot] = size_;
  dense_to_slot_[size_] = slot;
  ++size_;
  return EntityHandle{slot, generations_[slot]};
}

bool EntitySlots::destroy(EntityHandle handle) {
  if (!isLive(handle) || pending_flags_[handle.index] != 0U) {
    return false;
  }
  pending_flags_[handle.index] = 1U;
  pending_.push_back(slot_to_dense_[handle.index]);
  return true;
}

std::optional<uint32_t> EntitySlots::denseIndex(EntityHandle handle) const {
  if (!isLive(handle)) {
    return std::nullopt;
  }
  return slot_to_dense_[handle.index];
}

EntityHandle EntitySlots::handleAt(uint32_t dense_index) const {
  // ENGINE_ASSERT expands to the do-while statement-macro idiom.
  // NOLINTNEXTLINE(cppcoreguidelines-avoid-do-while)
  ENGINE_ASSERT(dense_index < size_, "EntitySlots::handleAt past size()");
  const uint32_t slot = dense_to_slot_[dense_index];
  return EntityHandle{slot, generations_[slot]};
}

bool EntitySlots::isPendingDestroy(EntityHandle handle) const {
  return isLive(handle) && pending_flags_[handle.index] != 0U;
}

std::span<const SlotMove> EntitySlots::compact() {
  moves_.clear();
  // Highest first: every index above the one being removed has already been
  // dealt with, so the last live entity is never itself waiting to go.
  std::ranges::sort(pending_, std::greater<>());
  for (const uint32_t dense_index : pending_) {
    removeAt(dense_index);
  }
  pending_.clear();
  return moves_;
}

void EntitySlots::hashInto(StateHasher& hasher) const {
  hasher.add(size_);
  hasher.addSpan(std::span<const uint32_t>(dense_to_slot_.data(), size_));
  hasher.addSpan(std::span<const uint32_t>(generations_));
  hasher.addSpan(std::span<const uint32_t>(free_slots_));
  hasher.addSpan(std::span<const uint32_t>(pending_));
}

uint32_t EntitySlots::capacity() const {
  return static_cast<uint32_t>(generations_.size());
}

bool EntitySlots::isLive(EntityHandle handle) const {
  if (handle.index >= capacity() ||
      generations_[handle.index] != handle.generation) {
    return false;
  }
  const uint32_t dense_index = slot_to_dense_[handle.index];
  return dense_index < size_ && dense_to_slot_[dense_index] == handle.index;
}

void EntitySlots::removeAt(uint32_t dense_index) {
  const uint32_t slot = dense_to_slot_[dense_index];
  const uint32_t last = size_ - 1U;
  if (dense_index != last) {
    const uint32_t moved_slot = dense_to_slot_[last];
    dense_to_slot_[dense_index] = moved_slot;
    slot_to_dense_[moved_slot] = dense_index;
    moves_.push_back(SlotMove{last, dense_index});
  }
  --size_;
  retire(slot);
}

void EntitySlots::retire(uint32_t slot) {
  pending_flags_[slot] = 0U;
  generations_[slot] = nextGeneration(generations_[slot]);
  slot_to_dense_[slot] = NO_DENSE_INDEX;
  free_slots_.push_back(slot);
}

}  // namespace eng::sim
