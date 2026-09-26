#include <array>
#include <catch2/catch_test_macros.hpp>
#include <engine/net/net-codec.h>
#include <engine/net/net-password.h>
#include <string>
#include <type_traits>
#include <vector>

using namespace eng;
using namespace eng::net;

namespace {

sim::PlayerInput sampleInput() {
  return {-32767, 32767, -1, 0, 0x80000001U, 7};
}

NetStart sampleStart() {
  NetStart start;
  start.run = 3;
  start.header.level_id = "arena";
  start.header.content_hash = 0xDEADBEEFCAFEF00DULL;
  start.header.seed = 42;
  start.header.player_count = 3;
  start.header.characters = {"scout", "", "tank", ""};
  start.input_delay = 2;
  return start;
}

NetFrame sampleFrame() {
  NetFrame frame{5, 123456789, 0b0100, {}};
  frame.input.players[0] = sampleInput();
  frame.input.players[3].move_x = -32768;
  return frame;
}

NetHashReport sampleReport() {
  NetHashReport report{1, {}};
  report.hash.tick = 600;
  report.hash.combined = 99;
  report.hash.section_count = 3;
  report.hash.sections[0].hash = 1;
  report.hash.sections[1].hash = 2;
  report.hash.sections[2].hash = UINT64_MAX;
  return report;
}

NetTrace sampleTrace() {
  NetTrace trace{2, {"players", "logic"}, {}};
  for (uint64_t tick = 100; tick < 103; ++tick) {
    NetHashReport report = sampleReport();
    report.hash.tick = tick;
    trace.ticks.push_back(report.hash);
  }
  return trace;
}

/// One of every message, with values at the edges of their ranges.
std::vector<NetMessage> everyMessage() {
  return {NetHello{NET_PROTOCOL_VERSION, 77, 0xB111D, 0xFA55, "scout"},
          NetWelcome{3},
          NetRefusal{NetRefusalReason::STALLED},
          NetRoster{0b1011},
          sampleStart(),
          NetInput{2, 1'000'000'000'000ULL, sampleInput()},
          sampleFrame(),
          sampleReport(),
          NetDesync{4, 60, 2, NET_SERVER_SLOT},
          NetEnd{65535},
          NetWaiting{3, 999, 0b0110},
          sampleTrace()};
}

bool sameReport(const NetHashReport& a, const NetHashReport& b) {
  if (a.run != b.run || a.hash.tick != b.hash.tick ||
      a.hash.combined != b.hash.combined ||
      a.hash.section_count != b.hash.section_count) {
    return false;
  }
  for (std::size_t i = 0; i < a.hash.section_count; ++i) {
    if (a.hash.sections[i].hash != b.hash.sections[i].hash) {
      return false;
    }
  }
  return true;
}

bool sameTrace(const NetTrace& a, const NetTrace& b) {
  if (a.run != b.run || a.section_names != b.section_names ||
      a.ticks.size() != b.ticks.size()) {
    return false;
  }
  for (std::size_t i = 0; i < a.ticks.size(); ++i) {
    if (!sameReport({0, a.ticks[i]}, {0, b.ticks[i]})) {
      return false;
    }
  }
  return true;
}

bool same(const NetMessage& a, const NetMessage& b) {
  if (a.index() != b.index()) {
    return false;
  }
  return std::visit(
      [&b](const auto& body) {
        using Body = std::decay_t<decltype(body)>;
        if constexpr (std::is_same_v<Body, NetHashReport>) {
          return sameReport(body, std::get<NetHashReport>(b));
        } else if constexpr (std::is_same_v<Body, NetTrace>) {
          return sameTrace(body, std::get<NetTrace>(b));
        } else {
          return body == std::get<Body>(b);
        }
      },
      a);
}

}  // namespace

TEST_CASE("every net message decodes to what was encoded") {
  for (const NetMessage& message : everyMessage()) {
    const std::vector<std::byte> bytes = encodeNetMessage(message);
    const std::optional<NetMessage> decoded = decodeNetMessage(bytes);
    REQUIRE(decoded);
    CHECK(same(*decoded, message));
  }
}

TEST_CASE("a net message's kind is its first byte") {
  const auto bytes = encodeNetMessage(NetEnd{1});
  CHECK(bytes.front() == std::byte{9});
}

TEST_CASE("every strict prefix of a net message is refused") {
  for (const NetMessage& message : everyMessage()) {
    const std::vector<std::byte> bytes = encodeNetMessage(message);
    for (std::size_t size = 0; size < bytes.size(); ++size) {
      CHECK_FALSE(decodeNetMessage(std::span(bytes.data(), size)));
    }
  }
}

TEST_CASE("a net message with a byte left over is refused") {
  for (const NetMessage& message : everyMessage()) {
    std::vector<std::byte> bytes = encodeNetMessage(message);
    bytes.push_back(std::byte{0});
    CHECK_FALSE(decodeNetMessage(bytes));
  }
}

TEST_CASE("net messages out of range are refused") {
  CHECK_FALSE(decodeNetMessage(std::vector{std::byte{12}}));
  CHECK_FALSE(decodeNetMessage(std::vector{std::byte{1}, std::byte{4}}));
  CHECK_FALSE(decodeNetMessage(std::vector{std::byte{2}, std::byte{6}}));
  NetStart start = sampleStart();
  start.header.player_count = 5;
  CHECK_FALSE(decodeNetMessage(encodeNetMessage(start)));
  NetHello hello;
  hello.character = std::string(NET_MAX_ID_BYTES + 1, 'x');
  CHECK_FALSE(decodeNetMessage(encodeNetMessage(hello)));
}

TEST_CASE("any single corrupted byte of a net message decodes safely") {
  for (const NetMessage& message : everyMessage()) {
    const std::vector<std::byte> bytes = encodeNetMessage(message);
    for (std::size_t i = 0; i < bytes.size(); ++i) {
      for (const uint8_t value :
           std::array<uint8_t, 4>{0x00, 0x7F, 0x80, 0xFF}) {
        std::vector<std::byte> corrupt = bytes;
        corrupt[i] = std::byte{value};
        (void)decodeNetMessage(corrupt);  // Must not read out of bounds.
      }
    }
  }
  SUCCEED();
}

TEST_CASE("a trace longer than a peer keeps is refused") {
  NetTrace trace = sampleTrace();
  trace.ticks.resize(NET_TRACE_TICKS + 1, trace.ticks.front());
  CHECK_FALSE(decodeNetMessage(encodeNetMessage(trace)));
}

TEST_CASE("the largest trace fits a message, and nothing larger is read") {
  NetTrace trace{1,
                 std::vector<std::string>(sim::MAX_TICK_HASH_SECTIONS,
                                          std::string(64, 'n')),
                 {}};
  sim::TickHash hash;
  hash.tick = UINT64_MAX;
  hash.section_count = sim::MAX_TICK_HASH_SECTIONS;
  trace.ticks.assign(NET_TRACE_TICKS, hash);
  CHECK(encodeNetMessage(trace).size() <= NET_MAX_MESSAGE_BYTES);
  const std::vector<std::byte> huge(NET_MAX_MESSAGE_BYTES + 1, std::byte{0});
  CHECK_FALSE(decodeNetMessage(huge));
}

TEST_CASE("a password digests to nothing only when there is none") {
  CHECK(netPasswordDigest("") == 0);
  CHECK(netPasswordDigest("a") != 0);
  CHECK(netPasswordDigest("a") != netPasswordDigest("b"));
}
