#pragma once

/// @file byte-reader.h
/// @brief Reads what `ByteWriter` writes, refusing to read past the end.
/// @par Threading
/// A value type over a borrowed buffer.

#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>

namespace eng::sim {

/// The replay decoder's input. Every read either returns a value and moves
/// on, or returns nothing and leaves the reader where it was — a replay file
/// is untrusted input, arriving from crash reports and other machines.
class ByteReader {
public:
  /// A reader over `data`, which must outlive it.
  explicit ByteReader(std::span<const std::byte> data);

  /// One byte.
  std::optional<uint8_t> u8();

  /// Two bytes, little-endian.
  std::optional<uint16_t> u16();

  /// Eight bytes, little-endian.
  std::optional<uint64_t> u64();

  /// A LEB128 varint of at most ten bytes.
  std::optional<uint64_t> varint();

  /// The next `count` bytes, unparsed.
  std::optional<std::span<const std::byte>> bytes(std::size_t count);

  /// True when every byte has been read.
  [[nodiscard]] bool atEnd() const { return offset_ == data_.size(); }

  /// True once any read has asked for bytes past the end — the difference
  /// between a truncated buffer and a malformed one.
  [[nodiscard]] bool overran() const { return overran_; }

private:
  /// The buffer being read.
  std::span<const std::byte> data_;
  /// Bytes consumed so far.
  std::size_t offset_ = 0;
  /// Set by the first read that ran out of bytes.
  bool overran_ = false;
};

}  // namespace eng::sim
