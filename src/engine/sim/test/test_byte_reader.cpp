#include "byte-reader.h"
#include "byte-writer.h"

#include <array>
#include <catch2/catch_test_macros.hpp>
#include <cstdint>
#include <vector>

using eng::sim::ByteReader;
using eng::sim::ByteWriter;

namespace {

std::vector<std::byte> bytesOf(std::initializer_list<uint8_t> values) {
  std::vector<std::byte> out;
  for (const uint8_t value : values) {
    out.push_back(std::byte{value});
  }
  return out;
}

/// Bytes as integers, which Catch2 can print when a comparison fails.
std::vector<uint8_t> integersOf(const std::vector<std::byte>& bytes) {
  std::vector<uint8_t> out;
  for (const std::byte byte : bytes) {
    out.push_back(std::to_integer<uint8_t>(byte));
  }
  return out;
}

}  // namespace

TEST_CASE("ByteWriter writes fixed-width integers little-endian") {
  ByteWriter out;
  out.u16(0x0102);
  out.u64(0x0102030405060708ULL);
  CHECK(integersOf(out.take()) == std::vector<uint8_t>{0x02, 0x01, 0x08, 0x07,
                                                       0x06, 0x05, 0x04, 0x03,
                                                       0x02, 0x01});
}

TEST_CASE("Varints round-trip at every width boundary") {
  const std::array<uint64_t, 7> values = {0,     127,        128,       16383,
                                          16384, UINT32_MAX, UINT64_MAX};
  ByteWriter out;
  for (const uint64_t value : values) {
    out.varint(value);
  }
  const auto bytes = out.take();
  ByteReader in(bytes);
  for (const uint64_t value : values) {
    CHECK(in.varint() == value);
  }
  CHECK(in.atEnd());
}

TEST_CASE("A varint is one byte below 128 and ten at most") {
  ByteWriter small;
  small.varint(127);
  CHECK(small.take().size() == 1);
  ByteWriter large;
  large.varint(UINT64_MAX);
  CHECK(large.take().size() == 10);
}

TEST_CASE("ByteReader refuses to read past the end, and says so") {
  const auto bytes = bytesOf({0x01});
  ByteReader in(bytes);
  CHECK_FALSE(in.u16().has_value());
  CHECK(in.overran());
  CHECK(in.u8() == 1);
}

TEST_CASE("A varint cut off partway is an overrun") {
  const auto bytes = bytesOf({0x80, 0x80});
  ByteReader in(bytes);
  CHECK_FALSE(in.varint().has_value());
  CHECK(in.overran());
}

TEST_CASE("A varint longer than 64 bits is refused without an overrun") {
  const auto eleven = bytesOf(
      {0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x01});
  ByteReader too_long(eleven);
  CHECK_FALSE(too_long.varint().has_value());
  CHECK_FALSE(too_long.overran());

  const auto overflowing =
      bytesOf({0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0x02});
  ByteReader wide(overflowing);
  CHECK_FALSE(wide.varint().has_value());
}
