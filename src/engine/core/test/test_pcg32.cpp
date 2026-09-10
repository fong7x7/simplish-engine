#include <array>
#include <catch2/catch_test_macros.hpp>
#include <cstdint>
#include <engine/core/pcg32.h>

using eng::Pcg32;

TEST_CASE("Pcg32 matches the reference implementation's published output") {
  // pcg32-demo seeds with pcg32_srandom_r(&rng, 42u, 54u) and prints these.
  // Matching them is what makes a seed mean the same thing on every platform.
  Pcg32 rng(42U, 54U);
  constexpr std::array<uint32_t, 6> EXPECTED = {
      0xa15c02b7U, 0x7b47f409U, 0xba1d3330U,
      0x83d2f293U, 0xbfa4784bU, 0xcbed606eU,
  };
  for (const uint32_t value : EXPECTED) {
    CHECK(rng.next() == value);
  }
}

TEST_CASE("Pcg32 streams with one seed produce different sequences") {
  Pcg32 spawn(7U, 1U);
  Pcg32 loot(7U, 2U);
  int same = 0;
  for (int i = 0; i < 64; ++i) {
    same += spawn.next() == loot.next() ? 1 : 0;
  }
  CHECK(same < 4);
}

TEST_CASE("Pcg32 with one seed and stream repeats exactly") {
  Pcg32 a(123U, 9U);
  Pcg32 b(123U, 9U);
  for (int i = 0; i < 1000; ++i) {
    REQUIRE(a.next() == b.next());
  }
  CHECK(a == b);
}

TEST_CASE("Pcg32::nextBelow stays inside its bound and reaches every value") {
  Pcg32 rng(1U, 1U);
  std::array<int, 6> counts{};
  for (int i = 0; i < 6000; ++i) {
    const uint32_t value = rng.nextBelow(6U);
    REQUIRE(value < 6U);
    ++counts.at(value);
  }
  for (const int count : counts) {
    CHECK(count > 800);
  }
}

TEST_CASE("Pcg32::nextBelow of one is always zero") {
  Pcg32 rng(5U, 5U);
  for (int i = 0; i < 32; ++i) {
    CHECK(rng.nextBelow(1U) == 0U);
  }
}

TEST_CASE("Pcg32::nextUnitFloat is at least zero and below one") {
  Pcg32 rng(99U, 3U);
  for (int i = 0; i < 10000; ++i) {
    const float value = rng.nextUnitFloat();
    REQUIRE(value >= 0.0F);
    REQUIRE(value < 1.0F);
  }
}
