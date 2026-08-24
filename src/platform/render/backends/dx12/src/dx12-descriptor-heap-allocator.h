#pragma once

#include <cstdint>
#include <vector>

namespace eng::render {

// ============================================================================
// DESIGN SUMMARY
// ============================================================================
// Dx12DescriptorHeapAllocator: Simple linear allocator for descriptor indices.
//
// Responsibilities:
// - Allocate sequential indices into a D3D12 descriptor heap
// - Recycle freed indices via free list
//
// Key Invariants:
// - Index 0 is valid (unlike handle table where 0 is sentinel)
// - Thread safety: main-thread-only
// ============================================================================

class Dx12DescriptorHeapAllocator {
public:
  /// Constructs an allocator for a heap of the given capacity.
  explicit Dx12DescriptorHeapAllocator(uint32_t capacity)
    : capacity_(capacity) {}

  Dx12DescriptorHeapAllocator() = default;

  /// Allocates a descriptor index. Returns UINT32_MAX on exhaustion.
  uint32_t allocate() {
    if (!free_list_.empty()) {
      uint32_t index = free_list_.back();
      free_list_.pop_back();
      return index;
    }
    if (next_index_ >= capacity_) {
      return UINT32_MAX;
    }
    return next_index_++;
  }

  /// Returns a descriptor index to the free list.
  void free(uint32_t index) { free_list_.push_back(index); }

  /// Returns the total capacity of the heap.
  uint32_t capacity() const { return capacity_; }

private:
  /// Total number of descriptors in the heap.
  uint32_t capacity_ = 0;
  /// Next unallocated index.
  uint32_t next_index_ = 0;
  /// Stack of freed indices for recycling.
  std::vector<uint32_t> free_list_;
};

}  // namespace eng::render
