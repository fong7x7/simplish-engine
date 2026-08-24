#pragma once

#include <cstdint>
#include <optional>
#include <utility>
#include <vector>

namespace eng::render {

// ============================================================================
// DESIGN SUMMARY
// ============================================================================
// Dx12HandleTable: Generational handle table for DX12 GPU resources.
//
// Responsibilities:
// - Map opaque uint64_t handles to backend-specific resource data
// - Detect use-after-free via generation counter in the handle
// - Recycle slots for constant-time insert/remove
//
// Key Invariants:
// - Handle 0 is always invalid (reserved sentinel)
// - Each slot has a generation counter; stale handles are rejected
// - remove() marks the slot as free and bumps the generation
// - Thread safety: main-thread-only (same as RhiDevice)
//
// Handle encoding: [generation:32 | index:32]
// ============================================================================

/// Number of bits used for the slot index within a handle.
constexpr uint32_t DX12_HANDLE_INDEX_BITS = 32;

/// Bitmask to extract the slot index from a handle.
constexpr uint64_t DX12_HANDLE_INDEX_MASK = 0xFFFFFFFF;

template <typename T> class Dx12HandleTable {
public:
  /// Inserts a resource and returns its opaque handle.
  uint64_t insert(T resource) {
    uint32_t index = allocateSlot();
    auto& slot = slots_[index];
    slot.resource = std::move(resource);
    slot.alive = true;
    return encode(slot.generation, index);
  }

  /// Looks up a resource by handle. Returns nullptr if invalid or stale.
  T* lookup(uint64_t handle) {
    auto [gen, index] = decode(handle);
    if (index >= slots_.size()) {
      return nullptr;
    }
    auto& slot = slots_[index];
    if (!slot.alive || slot.generation != gen) {
      return nullptr;
    }
    return &slot.resource;
  }

  /// Removes a resource by handle. Returns the resource if valid.
  std::optional<T> remove(uint64_t handle) {
    auto [gen, index] = decode(handle);
    if (index >= slots_.size()) {
      return std::nullopt;
    }
    auto& slot = slots_[index];
    if (!slot.alive || slot.generation != gen) {
      return std::nullopt;
    }
    slot.alive = false;
    ++slot.generation;
    free_list_.push_back(index);
    return std::move(slot.resource);
  }

  /// Calls a visitor on every alive resource. Visitor: void(T&).
  template <typename Fn> void forEachAlive(Fn&& visitor) {
    for (auto& slot : slots_) {
      if (slot.alive) {
        std::forward<Fn>(visitor)(slot.resource);
      }
    }
  }

private:
  /// A single slot in the handle table.
  struct Slot {
    /// The stored resource data.
    T resource{};
    /// Generation counter for stale-handle detection.
    uint32_t generation = 1;
    /// Whether this slot currently holds a live resource.
    bool alive = false;
  };

  /// Allocates a slot, recycling from the free list or appending a new one.
  uint32_t allocateSlot() {
    if (!free_list_.empty()) {
      uint32_t index = free_list_.back();
      free_list_.pop_back();
      return index;
    }
    auto index = static_cast<uint32_t>(slots_.size());
    slots_.push_back({});
    return index;
  }

  /// Encodes a generation and index into a uint64_t handle.
  static uint64_t encode(uint32_t generation, uint32_t index) {
    return (static_cast<uint64_t>(generation) << DX12_HANDLE_INDEX_BITS) |
           static_cast<uint64_t>(index);
  }

  /// Decoded handle components: generation and index.
  struct Decoded {
    /// Generation counter from the handle.
    uint32_t generation = 0;
    /// Slot index from the handle.
    uint32_t index = 0;
  };

  /// Decodes a uint64_t handle into generation and index.
  static Decoded decode(uint64_t handle) {
    auto index = static_cast<uint32_t>(handle & DX12_HANDLE_INDEX_MASK);
    auto generation = static_cast<uint32_t>(handle >> DX12_HANDLE_INDEX_BITS);
    return {generation, index};
  }

  /// Array of resource slots.
  std::vector<Slot> slots_;
  /// Stack of free slot indices for recycling.
  std::vector<uint32_t> free_list_;
};

}  // namespace eng::render
