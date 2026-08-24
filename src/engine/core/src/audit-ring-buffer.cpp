#include <engine/core/audit/audit-ring-buffer.h>
#include <memory>

namespace eng {

std::optional<AuditRingBuffer>
AuditRingBuffer::create(uint32_t capacity_bytes) {
  if (capacity_bytes == 0) {
    return std::nullopt;
  }
  AuditRingBuffer buf;
  buf.data = std::make_unique<std::byte[]>(capacity_bytes);
  buf.capacity = capacity_bytes;
  return buf;
}

std::byte* AuditRingBuffer::acquireSlot(AuditRingBuffer& buffer,
                                        uint32_t size) {
  if (size == 0) {
    return nullptr;
  }
  if (size > buffer.capacity) {
    buffer.overflow_count.fetch_add(1);
    return nullptr;
  }
  const uint32_t cursor = buffer.write_cursor.fetch_add(size);
  if (cursor + size > buffer.capacity) {
    buffer.overflow_count.fetch_add(1);
    return nullptr;
  }
  return buffer.data.get() + cursor;
}

void AuditRingBuffer::commitSlot(AuditRingBuffer& buffer) {
  buffer.commit_cursor.store(buffer.write_cursor.load());
}

void AuditRingBuffer::destroy(AuditRingBuffer& buffer) {
  buffer.data.reset();
  buffer.capacity = 0;
}

AuditRingBuffer& AuditRingBuffer::operator=(AuditRingBuffer&& other) noexcept {
  data = std::move(other.data);
  capacity = other.capacity;
  write_cursor.store(other.write_cursor.load());
  commit_cursor.store(other.commit_cursor.load());
  read_cursor = other.read_cursor;
  overflow_count.store(other.overflow_count.load());
  other.capacity = 0;
  return *this;
}

}  // namespace eng
