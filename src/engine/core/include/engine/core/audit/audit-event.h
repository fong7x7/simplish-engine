#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <span>
#include <string_view>

namespace eng {

// ============================================================================
// DESIGN SUMMARY -- Audit Event
// Technical Approach: docs/technical-approaches/engine/audit/format-emission.md
//
// Behaviours:
//   - AuditPayloadWriter writes typed fields directly into ring buffer memory
//   - Supports all payload field types: integers, floats, bool, vec3, quat,
//     colour, string16, bytes, entity_id, item_id, error_code
//   - Tracks bytes written and remaining capacity
//   - AuditEventRecord provides read access to a completed event (header +
//   payload)
//
// Edge Cases:
//   - Write exceeds remaining capacity: stop writing, no crash
//   - Empty string: writes uint16 length = 0
//   - String > 65533 bytes: truncated to 65533 in release
//
// Invariants:
//   - AuditPayloadWriter writes directly into caller-provided memory (no copy)
//   - bytesWritten() always reflects actual bytes written
//   - No heap allocation during writes
//
// Integration Points:
//   - audit-emit.h: AUDIT_EVENT macro constructs AuditPayloadWriter over slot
//   - audit-ring-buffer.h: slot memory provided by acquireSlot
//   - audit-query.h: AuditEventRecord used in query results
// ============================================================================

/// Encoded boolean for audit payload fields (1 byte on wire).
enum class AuditPayloadBool : uint8_t {
  FALSE,
  TRUE,
};

// --- Payload writer (writes directly into ring buffer slot) ---

class AuditPayloadWriter {
public:
  /// Construct a writer over a pre-allocated byte range.
  /// @param data Pointer to the start of the payload region.
  /// @param capacity Maximum bytes available for the payload.
  AuditPayloadWriter(std::byte* data, uint16_t capacity)
    : data_(data), capacity_(capacity) {}

  /// Write an unsigned 8-bit integer.
  void writeUint8(uint8_t value);

  /// Write an unsigned 16-bit integer (little-endian).
  void writeUint16(uint16_t value);

  /// Write an unsigned 32-bit integer (little-endian).
  void writeUint32(uint32_t value);

  /// Write an unsigned 64-bit integer (little-endian).
  void writeUint64(uint64_t value);

  /// Write a signed 8-bit integer.
  void writeInt8(int8_t value);

  /// Write a signed 16-bit integer (little-endian).
  void writeInt16(int16_t value);

  /// Write a signed 32-bit integer (little-endian).
  void writeInt32(int32_t value);

  /// Write a signed 64-bit integer (little-endian).
  void writeInt64(int64_t value);

  /// Write a 32-bit IEEE 754 float (little-endian).
  void writeFloat32(float value);

  /// Write a 64-bit IEEE 754 float (little-endian).
  void writeFloat64(double value);

  /// Write a boolean (1 byte: 0 = false, 1 = true).
  void writeBool(AuditPayloadBool value);

  /// Write a Vec3 (three float32 values: x, y, z; 12 bytes).
  void writeVec3(float x, float y, float z);

  /// Write a quaternion (four float32 values: x, y, z, w; 16 bytes).
  void writeQuat(std::array<float, 4> xyzw);

  /// Write an RGBA colour (4 bytes).
  void writeColour(std::array<uint8_t, 4> rgba);

  /// Write a length-prefixed UTF-8 string (uint16 length + bytes).
  void writeString16(std::string_view str);

  /// Write a length-prefixed raw byte blob (uint16 length + bytes).
  void writeBytes(std::span<const std::byte> blob);

  /// Write an entity ID (int64).
  void writeEntityId(int64_t entity_id);

  /// Write an item ID (same encoding as string16).
  void writeItemId(std::string_view item_id);

  /// Write an error code (uint32).
  void writeErrorCode(uint32_t code);

  /// Number of bytes written so far.
  uint16_t bytesWritten() const { return offset_; }

  /// Remaining capacity in bytes.
  uint16_t remaining() const {
    return static_cast<uint16_t>(capacity_ - offset_);
  }

private:
  /// Write raw bytes into the buffer, advancing offset.
  void writeRaw(const void* src, uint16_t size);

  /// Pointer to the start of the payload memory region.
  std::byte* data_{};
  /// Maximum bytes available in the payload region.
  uint16_t capacity_{};
  /// Current write position within the payload region.
  uint16_t offset_ = 0;
};

}  // namespace eng
