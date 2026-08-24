#include <engine/core/audit/audit-emit.h>
#include <engine/core/audit/audit-event-header.h>
#include <engine/core/audit/audit-event.h>
#include <engine/core/audit/audit-ring-buffer.h>

namespace eng {

/// Global audit emission enabled flag (analogous to logging enable flag).
// NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables)
std::atomic<bool> g_audit_emit_enabled{true};

void setAuditEmitEnabled(AuditEmitToggle toggle) {
  const bool enabled = (toggle == AuditEmitToggle::ENABLED);
  g_audit_emit_enabled.store(enabled, std::memory_order_relaxed);
}

bool isAuditEmitEnabled() {
  return g_audit_emit_enabled.load(std::memory_order_relaxed);
}

namespace {

  /// Default per-thread buffer size: 1 MB.
  constexpr uint32_t DEFAULT_THREAD_BUFFER_SIZE = 1024 * 1024;

  /// Thread-local ring buffer for the calling thread (per-thread arena).
  // NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables)
  thread_local std::optional<AuditRingBuffer> t_owned_buffer;

  /// Thread-local pointer to the active ring buffer (per-thread arena).
  // NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables)
  thread_local AuditRingBuffer* t_ring_buffer = nullptr;

  /// Lazily create a default thread-local ring buffer if none registered.
  AuditRingBuffer* ensureThreadBuffer() {
    if (t_ring_buffer != nullptr) {
      return t_ring_buffer;
    }
    if (!t_owned_buffer) {
      t_owned_buffer = AuditRingBuffer::create(DEFAULT_THREAD_BUFFER_SIZE);
    }
    if (t_owned_buffer) {
      t_ring_buffer = &*t_owned_buffer;
    }
    return t_ring_buffer;
  }

  /// Maximum slot size: header + max payload, rounded up for alignment.
  constexpr uint32_t MAX_SLOT_SIZE =
      (sizeof(AuditEventHeader) + AUDIT_MAX_PAYLOAD_SIZE +
       alignof(AuditEventHeader) - 1) &
      ~(alignof(AuditEventHeader) - 1);

  /// Offset past the end of the last write to get start of current slot.
  constexpr uint32_t SLOT_START_OFFSET = 1;

  /// Fill the event header fields from emit parameters.
  void fillHeader(AuditEventHeader* header, const AuditEmitParams& params) {
    header->category = params.category;
    header->event_type = params.event_type;
    header->severity = params.severity;
    header->actor_id = params.actor_id;
    header->target_id = params.target_id;
    header->flags = params.flags;
    header->timestamp_ns = 0;
    header->frame_number = 0;
  }

  /// Write the payload using the caller's function, return bytes written.
  uint16_t writePayload(std::byte* slot, AuditPayloadFn payload_fn,
                        void* user_data) {
    auto* payload_data = slot + sizeof(AuditEventHeader);
    AuditPayloadWriter writer(payload_data, AUDIT_MAX_PAYLOAD_SIZE);
    if (payload_fn != nullptr) {
      payload_fn(writer, user_data);
    }
    return writer.bytesWritten();
  }

  /// Acquire a slot from the thread-local ring buffer.
  /// Returns the slot pointer and buffer offset, or nullopt on failure.
  struct SlotInfo {
    /// Pointer to the acquired slot memory.
    std::byte* slot;
    /// Byte offset of the slot within the ring buffer.
    uint32_t offset;
  };

  /// Try to acquire a slot in the thread ring buffer.
  std::optional<SlotInfo> acquireThreadSlot() {
    if (ensureThreadBuffer() == nullptr) {
      return std::nullopt;
    }
    auto* slot = AuditRingBuffer::acquireSlot(*t_ring_buffer, MAX_SLOT_SIZE);
    if (slot == nullptr) {
      return std::nullopt;
    }
    const uint32_t offset =
        t_ring_buffer->write_cursor.load() - MAX_SLOT_SIZE + SLOT_START_OFFSET;
    return SlotInfo{slot, offset};
  }

  /// Write header and payload into slot, then commit.
  void fillAndCommit(std::byte* slot, const AuditEmitParams& params,
                     AuditPayloadFn payload_fn, void* user_data) {
    auto* header = reinterpret_cast<AuditEventHeader*>(
        slot);  // NOLINT(cppcoreguidelines-pro-type-reinterpret-cast)
                // -- ring buffer layout
    fillHeader(header, params);
    header->payload_size = writePayload(slot, payload_fn, user_data);
    AuditRingBuffer::commitSlot(*t_ring_buffer);
  }

}  // namespace

AuditEmitResult emitAuditEvent(const AuditEmitParams& params,
                               AuditPayloadFn payload_fn, void* user_data) {
  if (!g_audit_emit_enabled.load(std::memory_order_relaxed)) {
    return {0, false};
  }
  auto slot_info = acquireThreadSlot();
  if (!slot_info) {
    return {0, false};
  }
  fillAndCommit(slot_info->slot, params, payload_fn, user_data);
  return {slot_info->offset, true};
}

AuditEmitResult emitAuditEventCausedBy(uint32_t /*cause_offset*/,
                                       const AuditEmitParams& params,
                                       AuditPayloadFn payload_fn,
                                       void* user_data) {
  auto modified = params;
  modified.flags = params.flags | AuditFlags::HAS_CAUSE;
  return emitAuditEvent(modified, payload_fn, user_data);
}

AuditEmitResult emitAuditEventError(uint32_t /*error_code*/,
                                    const AuditEmitParams& params,
                                    AuditPayloadFn payload_fn,
                                    void* user_data) {
  auto modified = params;
  modified.flags = params.flags | AuditFlags::HAS_ERROR;
  return emitAuditEvent(modified, payload_fn, user_data);
}

}  // namespace eng
