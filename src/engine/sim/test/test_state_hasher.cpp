#include <array>
#include <catch2/catch_test_macros.hpp>
#include <cstdint>
#include <engine/math/vec2.h>
#include <engine/sim/state-hasher.h>
#include <span>

using eng::Vec2;
using eng::sim::StateHasher;

namespace {

template <typename T> uint64_t hashOf(const T& value) {
  StateHasher hasher;
  hasher.add(value);
  return hasher.value();
}

}  // namespace

TEST_CASE("StateHasher is pinned: changing it invalidates every replay") {
  // Recorded replays carry checkpoint hashes. If this value changes, every
  // checkpoint ever recorded stops matching — so a change here must be
  // deliberate, and must come with a replay format version bump.
  StateHasher hasher;
  const std::array<uint32_t, 5> values = {1, 2, 3, 4, 5};
  hasher.addSpan(std::span<const uint32_t>(values));
  hasher.add(uint64_t{0xDEADBEEF});
  hasher.add(1.5F);
  CHECK(hasher.value() == 0x375F62FB2F941CE9ULL);
}

TEST_CASE("StateHasher gives equal state an equal hash") {
  CHECK(hashOf(uint64_t{42}) == hashOf(uint64_t{42}));
  CHECK(hashOf(Vec2{1.0F, 2.0F}) == hashOf(Vec2{1.0F, 2.0F}));
}

TEST_CASE("StateHasher distinguishes values that differ by one bit") {
  CHECK(hashOf(uint64_t{42}) != hashOf(uint64_t{43}));
  CHECK(hashOf(Vec2{1.0F, 2.0F}) != hashOf(Vec2{2.0F, 1.0F}));
}

TEST_CASE("StateHasher is sensitive to order") {
  StateHasher ab;
  ab.add(uint32_t{1});
  ab.add(uint32_t{2});
  StateHasher ba;
  ba.add(uint32_t{2});
  ba.add(uint32_t{1});
  CHECK(ab.value() != ba.value());
}

TEST_CASE("StateHasher is sensitive to how bytes were split") {
  const std::array<uint8_t, 2> pair = {1, 2};
  StateHasher together;
  together.addSpan(std::span<const uint8_t>(pair));
  StateHasher apart;
  apart.add(uint8_t{1});
  apart.add(uint8_t{2});
  CHECK(together.value() != apart.value());
}

TEST_CASE("StateHasher hashes bits, so -0.0 and +0.0 differ") {
  CHECK(hashOf(0.0F) != hashOf(-0.0F));
}

TEST_CASE("StateHasher notices an empty span") {
  StateHasher empty;
  empty.addSpan(std::span<const uint32_t>{});
  CHECK(empty.value() != StateHasher{}.value());
}

TEST_CASE("StateHasher covers every byte of a span, tail included") {
  std::array<uint8_t, 13> bytes{};
  const uint64_t before = [&] {
    StateHasher hasher;
    hasher.addSpan(std::span<const uint8_t>(bytes));
    return hasher.value();
  }();
  bytes[12] = 1;
  StateHasher after;
  after.addSpan(std::span<const uint8_t>(bytes));
  CHECK(after.value() != before);
}
