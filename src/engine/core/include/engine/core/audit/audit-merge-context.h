#pragma once

#include "audit-ring-buffer.h"

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <mutex>
#include <thread>
#include <vector>

namespace eng {

// ============================================================================
// DESIGN SUMMARY
// ============================================================================
// AuditMergeContext: Owns the merge thread and global merge buffer.
//
// A dedicated merge thread drains all registered per-thread ring buffers
// at 1 HZ and writes events into the global merge buffer. The merge buffer
// is the single source of truth for queries and disk persistence.
//
// Thread Safety:
// - buffer_list_mutex protects thread_buffers add/remove/snapshot only.
// - Merge thread is the sole consumer of per-thread and merge buffers.
// ============================================================================

struct AuditMergeContext {
  /// Registered per-thread ring buffers drained by the merge thread.
  std::vector<AuditRingBuffer*> thread_buffers;
  /// Mutex protecting thread_buffers add/remove/snapshot operations.
  std::mutex buffer_list_mutex;
  /// Owned global merge buffer that accumulates events from all threads.
  std::unique_ptr<std::byte[]> merge_buffer{};
  /// Total capacity of the merge buffer in bytes.
  uint32_t merge_buffer_capacity = 0;
  /// Write cursor into the merge buffer (advanced by the merge thread).
  std::atomic<uint32_t> merge_write_cursor{0};
  /// Whether the merge thread is currently running.
  std::atomic<bool> running{false};
  /// Dedicated merge thread that drains per-thread buffers at 1 HZ.
  std::thread merge_thread{};

  AuditMergeContext() = default;
  AuditMergeContext(const AuditMergeContext&) = delete;
  AuditMergeContext& operator=(const AuditMergeContext&) = delete;
  /// Move constructor transfers ownership of thread and buffer state.
  AuditMergeContext(AuditMergeContext&& other) noexcept
    : thread_buffers(std::move(other.thread_buffers)),
      merge_buffer(std::move(other.merge_buffer)),
      merge_buffer_capacity(other.merge_buffer_capacity),
      merge_write_cursor(other.merge_write_cursor.load()),
      running(other.running.load()),
      merge_thread(std::move(other.merge_thread)) {
    other.merge_buffer_capacity = 0;
  }
  /// Move assignment transfers ownership of thread and buffer state.
  AuditMergeContext& operator=(AuditMergeContext&& other) noexcept;

  // --- Merge context API ---

  /// Create the merge context with a global merge buffer of the given capacity.
  /// Returns nullopt if allocation fails.
  static std::optional<AuditMergeContext>
  create(uint32_t merge_buffer_capacity);

  /// Register a per-thread ring buffer with the merge context.
  /// Called during thread init. Thread-safe (takes buffer_list_mutex briefly).
  static void registerThreadBuffer(AuditMergeContext& ctx,
                                   AuditRingBuffer* buffer);

  /// Deregister a per-thread ring buffer. Drains remaining events first.
  /// Called during thread shutdown. Thread-safe.
  static void unregisterThreadBuffer(AuditMergeContext& ctx,
                                     AuditRingBuffer* buffer);

  /// Start the merge thread (1 HZ drain loop).
  static void startMergeThread(AuditMergeContext& ctx);

  /// Signal stop, drain remaining events from all buffers, join merge thread.
  static void stopMergeThread(AuditMergeContext& ctx);

  /// Destroy the merge context and free the global merge buffer.
  static void destroy(AuditMergeContext& ctx);
};

}  // namespace eng
