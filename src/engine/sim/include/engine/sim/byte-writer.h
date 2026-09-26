#pragma once

/// @file byte-writer.h
/// @brief Appends little-endian integers and varints to a byte buffer.
/// @par Threading
/// A value type.

#include <cstddef>
#include <cstdint>
#include <span>
#include <vector>

namespace eng::sim {

/// The replay encoder's output. Fixed-width integers are written
/// little-endian whatever the host is, so a replay file is the same bytes
/// on every platform (Engine REQUIREMENTS §7, "Save/replay portability").
class ByteWriter {
public:
  /// Appends one byte.
  void u8(uint8_t value);

  /// Appends two bytes, little-endian.
  void u16(uint16_t value);

  /// Appends eight bytes, little-endian.
  void u64(uint64_t value);

  /// Appends `value` as a LEB128 varint: seven bits a byte, low bits first.
  void varint(uint64_t value);

  /// Appends raw bytes.
  void bytes(std::span<const std::byte> data);

  /// The buffer, moved out; the writer is empty afterwards.
  std::vector<std::byte> take();

private:
  /// Bytes written so far.
  std::vector<std::byte> out_;
};

}  // namespace eng::sim
