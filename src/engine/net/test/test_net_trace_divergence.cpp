#include <catch2/catch_test_macros.hpp>
#include <engine/net/net-desync.h>
#include <engine/net/net-trace-divergence.h>
#include <vector>

using namespace eng;
using namespace eng::net;

namespace {

/// A hash of @p tick with two sections, `players` and `logic`.
sim::TickHash hashOf(uint64_t tick, uint64_t players, uint64_t logic) {
  sim::TickHash hash;
  hash.tick = tick;
  hash.section_count = 2;
  hash.sections[0].hash = players;
  hash.sections[1].hash = logic;
  hash.combined = (players * 31) + logic;
  return hash;
}

/// Seat @p slot's trace of ticks @p from to @p to, whose logic section
/// reads @p drift more from tick @p drifts_at on.
NetPeerTrace traceOf(uint8_t slot, uint64_t from, uint64_t to,
                     uint64_t drifts_at) {
  NetPeerTrace peer{slot, {0, {"players", "logic"}, {}}};
  for (uint64_t tick = from; tick <= to; ++tick) {
    peer.trace.ticks.push_back(
        hashOf(tick, tick, tick + (tick >= drifts_at ? 7 : 0)));
  }
  return peer;
}

}  // namespace

TEST_CASE("the first tick peers' traces disagree on is found, and its "
          "section named") {
  const std::vector<NetPeerTrace> traces = {traceOf(0, 10, 90, UINT64_MAX),
                                            traceOf(1, 5, 80, 37)};
  const auto divergence = findTraceDivergence(traces);
  REQUIRE(divergence);
  CHECK(divergence->tick == 37);
  CHECK(divergence->section == 1);
  CHECK(divergence->section_name == "logic");
  CHECK(divergence->first_slot == 0);
  CHECK(divergence->other_slot == 1);
}

TEST_CASE("traces that agree wherever they overlap find nothing") {
  const std::vector<NetPeerTrace> traces = {
      traceOf(0, 0, 50, 70), traceOf(NET_SERVER_SLOT, 20, 60, 70)};
  CHECK_FALSE(findTraceDivergence(traces));
  CHECK_FALSE(findTraceDivergence(std::vector<NetPeerTrace>{}));
}

TEST_CASE("peers disagreeing on how many sections there are are told "
          "apart") {
  NetPeerTrace other = traceOf(1, 0, 3, UINT64_MAX);
  other.trace.ticks[2].section_count = 1;
  other.trace.ticks[2].combined = 0;
  const std::vector<NetPeerTrace> traces = {traceOf(0, 0, 3, UINT64_MAX),
                                            other};
  const auto divergence = findTraceDivergence(traces);
  REQUIRE(divergence);
  CHECK(divergence->tick == 2);
  CHECK(divergence->section == NET_SECTION_COUNT_DIFFERS);
  CHECK(divergence->section_name.empty());
}
