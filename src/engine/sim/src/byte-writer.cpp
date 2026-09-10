#include "byte-writer.h"

#include <utility>

namespace eng::sim {

namespace {

  /// Low seven bits of a varint byte.
  constexpr uint64_t VARINT_PAYLOAD = 0x7FU;

  /// Set on every varint byte but the last.
  constexpr uint8_t VARINT_CONTINUE = 0x80U;

}  // namespace

void ByteWriter::u8(uint8_t value) {
  out_.push_back(std::byte{value});
}

void ByteWriter::u16(uint16_t value) {
  u8(static_cast<uint8_t>(value & 0xFFU));
  u8(static_cast<uint8_t>(value >> 8U));
}

void ByteWriter::u64(uint64_t value) {
  for (int i = 0; i < 8; ++i) {
    u8(static_cast<uint8_t>(value & 0xFFU));
    value >>= 8U;
  }
}

void ByteWriter::varint(uint64_t value) {
  while (value > VARINT_PAYLOAD) {
    u8(static_cast<uint8_t>((value & VARINT_PAYLOAD) | VARINT_CONTINUE));
    value >>= 7U;
  }
  u8(static_cast<uint8_t>(value));
}

void ByteWriter::bytes(std::span<const std::byte> data) {
  out_.insert(out_.end(), data.begin(), data.end());
}

std::vector<std::byte> ByteWriter::take() {
  return std::exchange(out_, {});
}

}  // namespace eng::sim
