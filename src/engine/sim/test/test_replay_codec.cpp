#include <catch2/catch_test_macros.hpp>
#include <cstdint>
#include <engine/sim/replay-codec.h>
#include <engine/sim/tick-hash-builder.h>

using eng::sim::decodeReplay;
using eng::sim::encodeReplay;
using eng::sim::Replay;
using eng::sim::ReplayDecodeError;
using eng::sim::TickHash;
using eng::sim::TickHashBuilder;
using eng::sim::TickInput;

namespace {

/// Two players whose inputs change in every way the format encodes: axes
/// both directions and across the full range, buttons on and off, and long
/// runs of held input between.
Replay sampleReplay() {
  Replay replay;
  replay.header = {"transit_station", 0xC0FFEEULL, 42, 2, {"scout", "tank"}};
  for (uint64_t tick = 0; tick < 300; ++tick) {
    TickInput input;
    input.players[0].move_x = tick < 100 ? int16_t{32767} : int16_t{-32767};
    input.players[0].aim_y = static_cast<int16_t>((tick * 331) % 20000);
    input.players[0].buttons = tick % 40 < 5 ? 0x80000001U : 0U;
    input.players[1].move_y = tick > 200 ? int16_t{-12} : int16_t{0};
    replay.inputs.push_back(input);
  }
  for (const uint64_t tick : {0ULL, 60ULL, 299ULL}) {
    TickHashBuilder builder;
    builder.section("a").add(tick);
    builder.section("b").add(tick * 3);
    replay.checkpoints.push_back(builder.finish(tick));
  }
  return replay;
}

/// A checkpoint as a decoded replay carries it: hashes without names.
TickHash unnamed(TickHash hash) {
  for (auto& section : hash.sections) {
    section.name = {};
  }
  return hash;
}

void requireSameCheckpoint(const TickHash& decoded, const TickHash& original) {
  REQUIRE(decoded.tick == original.tick);
  REQUIRE(decoded.combined == original.combined);
  REQUIRE(decoded.section_count == original.section_count);
  for (std::size_t i = 0; i < decoded.section_count; ++i) {
    REQUIRE(decoded.sections[i].hash == original.sections[i].hash);
    REQUIRE(decoded.sections[i].name == unnamed(original).sections[i].name);
  }
}

}  // namespace

TEST_CASE("A replay survives encoding and decoding unchanged") {
  const Replay replay = sampleReplay();
  const auto decoded = decodeReplay(encodeReplay(replay));
  REQUIRE(decoded.has_value());
  CHECK(decoded->header == replay.header);
  CHECK(decoded->inputs == replay.inputs);
  REQUIRE(decoded->checkpoints.size() == replay.checkpoints.size());
  for (std::size_t i = 0; i < replay.checkpoints.size(); ++i) {
    requireSameCheckpoint(decoded->checkpoints[i], replay.checkpoints[i]);
  }
}

TEST_CASE("An empty replay round-trips") {
  Replay replay;
  replay.header.level_id = "main";
  const auto decoded = decodeReplay(encodeReplay(replay));
  REQUIRE(decoded.has_value());
  CHECK(decoded->header == replay.header);
  CHECK(decoded->inputs.empty());
}

TEST_CASE("Ten minutes of held input encodes to a few bytes") {
  Replay replay;
  replay.header.player_count = 4;
  TickInput held;
  held.players[2].move_x = 32767;
  replay.inputs.assign(60U * 60U * 10U, held);
  CHECK(encodeReplay(replay).size() < 64);
}

TEST_CASE("Every truncation of a replay is reported as truncated") {
  const auto bytes = encodeReplay(sampleReplay());
  for (std::size_t length = 0; length < bytes.size(); ++length) {
    const auto decoded = decodeReplay(std::span(bytes).first(length));
    REQUIRE_FALSE(decoded.has_value());
    REQUIRE(decoded.error() == ReplayDecodeError::TRUNCATED);
  }
}

TEST_CASE("A replay with the wrong magic or version is refused") {
  auto bytes = encodeReplay(sampleReplay());
  auto wrong_magic = bytes;
  wrong_magic[0] = std::byte{'X'};
  CHECK(decodeReplay(wrong_magic).error() == ReplayDecodeError::BAD_MAGIC);

  auto wrong_version = bytes;
  wrong_version[4] = std::byte{99};
  CHECK(decodeReplay(wrong_version).error() ==
        ReplayDecodeError::UNSUPPORTED_VERSION);
}

TEST_CASE("Trailing bytes make a replay malformed") {
  auto bytes = encodeReplay(sampleReplay());
  bytes.push_back(std::byte{0});
  CHECK(decodeReplay(bytes).error() == ReplayDecodeError::MALFORMED);
}

TEST_CASE("A player count outside 1..4 is malformed") {
  auto bytes = encodeReplay(sampleReplay());
  bytes[6] = std::byte{0};
  CHECK(decodeReplay(bytes).error() == ReplayDecodeError::MALFORMED);
  bytes[6] = std::byte{5};
  CHECK(decodeReplay(bytes).error() == ReplayDecodeError::MALFORMED);
}

TEST_CASE("No corruption of a single byte crashes the decoder") {
  // Run under the asan preset to make this mean something: every outcome
  // is acceptable except reading out of bounds.
  const auto original = encodeReplay(sampleReplay());
  for (std::size_t i = 0; i < original.size(); ++i) {
    for (const uint8_t flip : {uint8_t{0x01}, uint8_t{0x80}, uint8_t{0xFF}}) {
      auto bytes = original;
      bytes[i] ^= std::byte{flip};
      (void)decodeReplay(bytes);
    }
  }
  SUCCEED();
}

TEST_CASE("Each player's character round-trips, and only the session's") {
  Replay replay;
  replay.header.player_count = 2;
  replay.header.characters = {"scout", "", "never", "written"};
  const auto decoded = decodeReplay(encodeReplay(replay));
  REQUIRE(decoded.has_value());
  CHECK(decoded->header.characters[0] == "scout");
  CHECK(decoded->header.characters[1].empty());
  // Slots past the player count are not part of the session.
  CHECK(decoded->header.characters[2].empty());
  CHECK(decoded->header.characters[3].empty());
}
