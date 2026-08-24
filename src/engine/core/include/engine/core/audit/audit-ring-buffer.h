#pragma once

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>

namespace eng {

// ============================================================================
// DESIGN SUMMARY -- Audit Ring Buffer
// Technical Approach:
// docs/technical-approaches/engine/audit/ring-buffer-persistence.md
//
// Behaviours:
//   - Allocate a per-thread ring buffer of configurable size
//   - Lock-free acquireSlot reserves contiguous byte range for writing
//   - commitSlot with atomic release fence makes event visible to merge thread
//   - Track overflow_count when buffer is full
//
// Edge Cases:
//   - Buffer full: acquireSlot returns nullptr, overflow_count incremented
//   - Thread exits with pending events: merge thread drains before deregister
//
// Invariants:
//   - Per-thread buffers are pre-allocated, no dynamic growth
//   - acquireSlot / commitSlot are lock-free (atomic CAS on write cursor)
//   - Merge thread is the only consumer of per-thread buffers
//
// Integration Points:
//   - audit-emit.h: thread-local tl_audit_buffer pointer for zero-copy writes
//   - audit-merge-context.h: owns AuditMergeContext lifecycle
//   - audit-persistence.h: merge buffer feeds disk flush
//   - audit-query.h: merge buffer is the data source for queries
// ============================================================================

// --- Per-thread ring buffer ---

struct AuditRingBuffer {
  /// Owned byte storage for the ring buffer; use `.get()` for raw pointers.
  std::unique_ptr<std::byte[]> data{};
  /// Total capacity of the ring buffer in bytes.
  uint32_t capacity = 0;
  /// Write cursor advanced by acquireSlot (lock-free, atomic CAS).
  std::atomic<uint32_t> write_cursor{0};
  /// Commit cursor advanced by commitSlot (atomic release fence).
  std::atomic<uint32_t> commit_cursor{0};
  /// Read cursor advanced only by the merge thread.
  uint32_t read_cursor = 0;
  /// Number of events dropped because the buffer was full.
  std::atomic<uint64_t> overflow_count{0};

  AuditRingBuffer() = default;
  AuditRingBuffer(const AuditRingBuffer&) = delete;
  AuditRingBuffer& operator=(const AuditRingBuffer&) = delete;
  /// Move constructor loads atomic values from source.
  AuditRingBuffer(AuditRingBuffer&& other) noexcept
    : data(std::move(other.data)), capacity(other.capacity),
      write_cursor(other.write_cursor.load()),
      commit_cursor(other.commit_cursor.load()), read_cursor(other.read_cursor),
      overflow_count(other.overflow_count.load()) {
    other.capacity = 0;
  }
  /// Move assignment loads atomic values from source.
  AuditRingBuffer& operator=(AuditRingBuffer&& other) noexcept;

  // --- Per-thread buffer API ---

  /// Allocate a per-thread ring buffer with the given capacity.
  /// Returns nullopt if allocation fails.
  static std::optional<AuditRingBuffer> create(uint32_t capacity_bytes);

  /// Reserve a contiguous byte range in the ring buffer for writing.
  /// Returns nullptr if the buffer is full (overflow_count incremented).
  /// Thread-safe. Lock-free.
  static std::byte* acquireSlot(AuditRingBuffer& buffer, uint32_t size);

  /// Commit the most recently acquired slot with an atomic release fence.
  /// Makes the written event visible to the merge thread.
  /// Thread-safe. Lock-free.
  static void commitSlot(AuditRingBuffer& buffer);

  /// Free the memory owned by a ring buffer.
  static void destroy(AuditRingBuffer& buffer);
};

}  // namespace eng
