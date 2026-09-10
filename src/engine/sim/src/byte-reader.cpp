#include "byte-reader.h"

namespace eng::sim {

namespace {

  /// Bytes a 64-bit LEB128 varint can occupy.
  constexpr unsigned MAX_VARINT_BYTES = 10;

  /// Low seven bits of a varint byte.
  constexpr uint8_t VARINT_PAYLOAD = 0x7FU;

  /// Set on every varint byte but the last.
  constexpr uint8_t VARINT_CONTINUE = 0x80U;

  /// `count` bytes of `data`, little-endian, as an integer.
  uint64_t littleEndian(std::span<const std::byte> data) {
    uint64_t value = 0;
    for (std::size_t i = data.size(); i > 0; --i) {
      value = (value << 8U) | std::to_integer<uint64_t>(data[i - 1]);
    }
    return value;
  }

  /// False when varint byte number `index` carries bits a 64-bit value has no
  /// room for: only the lowest payload bit of the tenth byte fits.
  bool varintByteFits(unsigned index, uint8_t byte) {
    return index + 1U < MAX_VARINT_BYTES || (byte & VARINT_PAYLOAD) <= 1U;
  }

}  // namespace

ByteReader::ByteReader(std::span<const std::byte> data) : data_(data) {}

std::optional<uint8_t> ByteReader::u8() {
  const auto data = bytes(1);
  if (!data) {
    return std::nullopt;
  }
  return static_cast<uint8_t>(littleEndian(*data));
}

std::optional<uint16_t> ByteReader::u16() {
  const auto data = bytes(2);
  if (!data) {
    return std::nullopt;
  }
  return static_cast<uint16_t>(littleEndian(*data));
}

std::optional<uint64_t> ByteReader::u64() {
  const auto data = bytes(8);
  if (!data) {
    return std::nullopt;
  }
  return littleEndian(*data);
}

std::optional<uint64_t> ByteReader::varint() {
  const std::size_t start = offset_;
  uint64_t value = 0;
  for (unsigned i = 0; i < MAX_VARINT_BYTES; ++i) {
    const auto byte = u8();
    if (!byte || !varintByteFits(i, *byte)) {
      break;
    }
    value |= static_cast<uint64_t>(*byte & VARINT_PAYLOAD) << (7U * i);
    if ((*byte & VARINT_CONTINUE) == 0U) {
      return value;
    }
  }
  offset_ = start;
  return std::nullopt;
}

std::optional<std::span<const std::byte>> ByteReader::bytes(std::size_t count) {
  if (count > data_.size() - offset_) {
    overran_ = true;
    return std::nullopt;
  }
  const auto result = data_.subspan(offset_, count);
  offset_ += count;
  return result;
}

}  // namespace eng::sim
