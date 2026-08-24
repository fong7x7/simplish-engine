#include <algorithm>
#include <cstring>
#include <engine/core/audit/audit-event.h>

namespace eng {

void AuditPayloadWriter::writeRaw(const void* src, uint16_t size) {
  if (offset_ + size > capacity_) {
    return;
  }
  std::memcpy(data_ + offset_, src, size);
  offset_ += size;
}

void AuditPayloadWriter::writeUint8(uint8_t value) {
  writeRaw(&value, sizeof(value));
}

void AuditPayloadWriter::writeUint16(uint16_t value) {
  writeRaw(&value, sizeof(value));
}

void AuditPayloadWriter::writeUint32(uint32_t value) {
  writeRaw(&value, sizeof(value));
}

void AuditPayloadWriter::writeUint64(uint64_t value) {
  writeRaw(&value, sizeof(value));
}

void AuditPayloadWriter::writeInt8(int8_t value) {
  writeRaw(&value, sizeof(value));
}

void AuditPayloadWriter::writeInt16(int16_t value) {
  writeRaw(&value, sizeof(value));
}

void AuditPayloadWriter::writeInt32(int32_t value) {
  writeRaw(&value, sizeof(value));
}

void AuditPayloadWriter::writeInt64(int64_t value) {
  writeRaw(&value, sizeof(value));
}

void AuditPayloadWriter::writeFloat32(float value) {
  writeRaw(&value, sizeof(value));
}

void AuditPayloadWriter::writeFloat64(double value) {
  writeRaw(&value, sizeof(value));
}

void AuditPayloadWriter::writeBool(AuditPayloadBool value) {
  const uint8_t byte = (value == AuditPayloadBool::TRUE) ? 1 : 0;
  writeRaw(&byte, sizeof(byte));
}

void AuditPayloadWriter::writeVec3(float x, float y, float z) {
  writeFloat32(x);
  writeFloat32(y);
  writeFloat32(z);
}

void AuditPayloadWriter::writeQuat(std::array<float, 4> xyzw) {
  for (float v : xyzw) {
    writeFloat32(v);
  }
}

void AuditPayloadWriter::writeColour(std::array<uint8_t, 4> rgba) {
  for (uint8_t v : rgba) {
    writeUint8(v);
  }
}

void AuditPayloadWriter::writeString16(std::string_view str) {
  if (remaining() < 2) {
    return;
  }
  const auto len = static_cast<uint16_t>(
      std::min(str.size(), static_cast<size_t>(remaining() - 2)));
  writeUint16(len);
  writeRaw(str.data(), len);
}

void AuditPayloadWriter::writeBytes(std::span<const std::byte> blob) {
  if (remaining() < 2) {
    return;
  }
  const auto len = static_cast<uint16_t>(
      std::min(blob.size(), static_cast<size_t>(remaining() - 2)));
  writeUint16(len);
  writeRaw(blob.data(), len);
}

void AuditPayloadWriter::writeEntityId(int64_t entity_id) {
  writeInt64(entity_id);
}

void AuditPayloadWriter::writeItemId(std::string_view item_id) {
  writeString16(item_id);
}

void AuditPayloadWriter::writeErrorCode(uint32_t code) {
  writeUint32(code);
}

}  // namespace eng
