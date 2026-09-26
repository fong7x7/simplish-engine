#include "support/loopback-session.h"

#include <catch2/catch_test_macros.hpp>
#include <engine/net/lockstep-server.h>
#include <engine/net/net-input-delay.h>
#include <engine/net/net-password.h>
#include <engine/net/net-trace-divergence.h>
#include <string>
#include <vector>

using namespace eng;
using namespace eng::net;
using eng::net::test::LoopbackSession;

namespace {

/// A server configured with @p delay ticks of input delay, keeping frames.
LockstepServerConfig keeping(uint8_t delay) {
  LockstepServerConfig config;
  config.input_delay = delay;
  config.frames = NetServerFrames::KEEP;
  return config;
}

sim::PlayerInput pressing(uint32_t buttons) {
  sim::PlayerInput input;
  input.buttons = buttons;
  return input;
}

/// Take every frame @p client has, and give how many there were.
int drain(LockstepClient& client) {
  int taken = 0;
  while (client.takeFrame()) {
    ++taken;
  }
  return taken;
}

/// A hash of @p tick whose one section is @p value.
sim::TickHash hashOf(uint64_t tick, uint64_t value) {
  sim::TickHash hash;
  hash.tick = tick;
  hash.combined = value;
  hash.section_count = 1;
  hash.sections[0] = {"state", value};
  return hash;
}

}  // namespace

TEST_CASE("a server seats clients in order and tells them who is here") {
  LoopbackSession session;
  LockstepClient& first =
      session.join({NET_PROTOCOL_VERSION, 0, 0, 0, "scout"});
  LockstepClient& second = session.join();
  session.pump();
  CHECK(first.slot() == 0);
  CHECK(second.slot() == 1);
  CHECK(first.state() == NetClientState::SEATED);
  CHECK(session.server().seated() == 0b11);
  CHECK(first.roster() == 0b11);
  CHECK(second.roster() == 0b11);
}

TEST_CASE("a server refuses another protocol, other content, or a full "
          "table") {
  LockstepServerConfig config;
  config.content_hash = 5;
  config.seats = 1;
  LoopbackSession session(config);
  LockstepClient& old = session.join({NET_PROTOCOL_VERSION + 1, 5, 0, 0, ""});
  LockstepClient& modded = session.join({NET_PROTOCOL_VERSION, 6, 0, 0, ""});
  LockstepClient& seated = session.join({NET_PROTOCOL_VERSION, 5, 0, 0, ""});
  LockstepClient& late = session.join({NET_PROTOCOL_VERSION, 5, 0, 0, ""});
  session.pump();
  CHECK(old.refusal() == NetRefusalReason::PROTOCOL);
  CHECK(modded.refusal() == NetRefusalReason::CONTENT);
  CHECK(seated.slot() == 0);
  CHECK(late.refusal() == NetRefusalReason::FULL);
  CHECK(late.state() == NetClientState::REFUSED);
  CHECK(session.server().seated() == 0b1);
}

TEST_CASE("a server starts nothing with nobody seated") {
  LoopbackSession session;
  CHECK_FALSE(session.server().start("arena", 1));
  CHECK(session.server().state() == NetServerState::LOBBY);
}

TEST_CASE("a start carries the run's header") {
  LockstepServerConfig config = keeping(2);
  config.content_hash = 9;
  LoopbackSession session(config);
  LockstepClient& scout =
      session.join({NET_PROTOCOL_VERSION, 9, 0, 0, "scout"});
  (void)session.join({NET_PROTOCOL_VERSION, 9, 0, 0, "tank"});
  session.pump();
  const auto start = session.server().start("arena", 42);
  session.pump();
  REQUIRE(start);
  const auto heard = scout.takeStart();
  REQUIRE(heard);
  CHECK(*heard == *start);
  CHECK(heard->header ==
        sim::ReplayHeader{"arena", 9, 42, 2, {"scout", "tank", "", ""}});
  CHECK_FALSE(scout.takeStart());
}

TEST_CASE("a run's first frames, before any input can arrive, follow its "
          "start at once") {
  LoopbackSession session(keeping(2));
  LockstepClient& client = session.join();
  session.pump();
  (void)session.start("arena", 42);
  CHECK(client.framesWaiting() == 2);
  CHECK(session.server().nextTick() == 2);
}

TEST_CASE("a server sends no frame until every player's input is in") {
  LoopbackSession session(keeping(1));
  LockstepClient& a = session.join();
  LockstepClient& b = session.join();
  session.pump();
  (void)session.start("arena", 0);
  CHECK(drain(a) == 1);
  CHECK(drain(b) == 1);
  REQUIRE(a.sendInput(pressing(1)));
  session.pump();
  CHECK(a.framesWaiting() == 0);
  CHECK(session.server().waitingOn() == 0b10);
  REQUIRE(b.sendInput(pressing(2)));
  session.pump();
  CHECK(session.server().waitingOn() == 0b11);
}

TEST_CASE("a frame carries every player's input for its tick, the same to "
          "each") {
  LoopbackSession session(keeping(1));
  LockstepClient& a = session.join();
  LockstepClient& b = session.join();
  session.pump();
  (void)session.start("arena", 0);
  (void)drain(a);
  (void)drain(b);
  (void)a.sendInput(pressing(1));
  (void)b.sendInput(pressing(2));
  session.pump();
  const auto frame = a.takeFrame();
  REQUIRE(frame);
  CHECK(frame->tick == 1);
  CHECK(frame->input.players[0].buttons == 1);
  CHECK(frame->input.players[1].buttons == 2);
  CHECK(b.takeFrame() == frame);
}

TEST_CASE("a dropped player is marked absent and the run goes on without "
          "them") {
  LoopbackSession session(keeping(1));
  LockstepClient& a = session.join();
  (void)session.join();
  session.pump();
  (void)session.start("arena", 0);
  (void)drain(a);
  session.leave(1);
  REQUIRE(a.sendInput(pressing(1)));
  session.pump();
  const auto frame = a.takeFrame();
  REQUIRE(frame);
  CHECK(frame->absent == 0b10);
  CHECK(session.server().playing() == 0b01);
  CHECK(a.roster() == 0b01);
}

TEST_CASE("a player who joins mid-run waits in their seat for the next "
          "start") {
  LoopbackSession session(keeping(1));
  LockstepClient& a = session.join();
  session.pump();
  (void)session.start("arena", 0);
  LockstepClient& late = session.join();
  session.pump();
  CHECK(late.slot() == 1);
  CHECK(late.state() == NetClientState::SEATED);
  CHECK(late.framesWaiting() == 0);
  const auto next = session.server().start("arena-2", 0);
  session.pump();
  CHECK(next->header.player_count == 2);
  CHECK(late.state() == NetClientState::PLAYING);
  CHECK(a.run()->run == late.run()->run);
}

TEST_CASE("an empty seat below a filled one plays absent from the start") {
  LoopbackSession session(keeping(1));
  (void)session.join();
  LockstepClient& b = session.join();
  session.pump();
  session.leave(0);
  session.pump();
  const auto start = session.server().start("arena", 0);
  session.pump();
  CHECK(start->header.player_count == 2);
  REQUIRE(b.takeFrame());
  REQUIRE(b.sendInput(pressing(4)));
  session.pump();
  const auto frame = b.takeFrame();
  REQUIRE(frame);
  CHECK(frame->absent == 0b01);
  CHECK(frame->input.players[1].buttons == 4);
}

TEST_CASE("a server keeps the frames it sends when asked, for its own "
          "simulation") {
  LoopbackSession session(keeping(2));
  (void)session.join();
  session.pump();
  (void)session.server().start("arena", 0);
  CHECK(session.server().takeFrame()->tick == 0);
  CHECK(session.server().takeFrame()->tick == 1);
  CHECK_FALSE(session.server().takeFrame());
}

TEST_CASE("a server sends no frame at or past where it was told to stop") {
  LoopbackSession session(keeping(1));
  LockstepClient& a = session.join();
  session.pump();
  (void)session.server().start("arena", 0);
  session.server().stopAt(2);
  session.pump();
  for (int tick = 0; tick < 4; ++tick) {
    (void)drain(a);
    (void)a.sendInput({});
    session.pump();
  }
  CHECK(session.server().nextTick() == 2);
  CHECK(a.nextFrameTick() == 2);
}

TEST_CASE("differing hashes halt the run for everyone, naming tick, section "
          "and seat") {
  LoopbackSession session(keeping(1));
  LockstepClient& a = session.join();
  LockstepClient& b = session.join();
  session.pump();
  (void)session.start("arena", 0);
  a.reportHash(hashOf(60, 1));
  session.pump();
  b.reportHash(hashOf(60, 2));
  session.pump();
  CHECK(session.server().state() == NetServerState::DESYNCED);
  const NetDesync expected{0, 60, 0, 1};
  CHECK(session.server().desync() == expected);
  CHECK(a.state() == NetClientState::DESYNCED);
  CHECK(b.desync() == expected);
  CHECK_FALSE(a.sendInput({}));
}

TEST_CASE("agreeing hashes, and ticks that are not compared, change nothing") {
  LoopbackSession session(keeping(1));
  LockstepClient& a = session.join();
  LockstepClient& b = session.join();
  session.pump();
  (void)session.start("arena", 0);
  session.server().reportHash(hashOf(120, 7));
  a.reportHash(hashOf(120, 7));
  b.reportHash(hashOf(120, 7));
  a.reportHash(hashOf(61, 1));
  b.reportHash(hashOf(61, 2));
  session.pump();
  CHECK(session.server().state() == NetServerState::RUNNING);
  b.reportHash(hashOf(120, 8));
  session.pump();
  CHECK(session.server().state() == NetServerState::DESYNCED);
}

TEST_CASE("a server's own hash, reported first, is the reference") {
  LoopbackSession session(keeping(1));
  LockstepClient& a = session.join();
  session.pump();
  (void)session.start("arena", 0);
  session.server().reportHash(hashOf(0, 3));
  a.reportHash(hashOf(0, 4));
  session.pump();
  REQUIRE(session.server().desync());
  CHECK(session.server().desync()->slot == 0);
}

TEST_CASE("ending a run returns everyone to their seats, and a new run "
          "ignores the old one's input") {
  LoopbackSession session(keeping(1));
  LockstepClient& a = session.join();
  session.pump();
  (void)session.start("arena", 0);
  (void)drain(a);
  session.server().end();
  session.pump();
  CHECK(a.state() == NetClientState::SEATED);
  CHECK(session.server().state() == NetServerState::LOBBY);
  const auto second = session.server().start("arena", 0);
  session.pump();
  CHECK(second->run == 1);
  CHECK(a.run()->run == 1);
  CHECK(a.state() == NetClientState::PLAYING);
}

TEST_CASE("a measured delay is chosen from the worst round trip seated") {
  LockstepServerConfig config = keeping(3);
  config.delay_choice = NetDelayChoice::MEASURED;
  LoopbackSession session(config);
  (void)session.join();
  session.pump();
  session.network().setRoundTrip(120);
  const auto slow = session.start("arena");
  session.network().setRoundTrip(1);
  const auto fast = session.start("arena");
  REQUIRE(slow);
  REQUIRE(fast);
  CHECK(slow->input_delay == 9);
  CHECK(fast->input_delay == NET_MIN_MEASURED_DELAY);
}

TEST_CASE("a server says who a stalled run is waiting on, and only then") {
  LoopbackSession session(keeping(1));
  session.seat(2);
  LockstepClient& a = session.client(0);
  (void)session.start("arena");
  (void)drain(a);
  (void)drain(session.client(1));
  (void)a.sendInput({});
  session.pump();
  const auto said = session.server().announceWaiting();
  session.pump();
  CHECK(said == NetWaiting{0, 1, 0b10});
  CHECK(a.waiting() == said);
  (void)session.client(1).sendInput({});
  session.pump();
  CHECK_FALSE(a.waiting());
}

TEST_CASE("after a desync every playing seat sends its trace") {
  LoopbackSession session(keeping(1));
  session.seat(2);
  (void)session.start("arena");
  for (uint64_t tick = 0; tick <= 60; ++tick) {
    session.client(0).reportHash(hashOf(tick, 1));
    session.client(1).reportHash(hashOf(tick, tick < 37 ? 1 : 2));
  }
  session.pump();
  REQUIRE(session.server().state() == NetServerState::DESYNCED);
  CHECK(session.server().tracesComplete());
  const auto traces = session.server().traces();
  REQUIRE(traces.size() == 2);
  CHECK(traces[1].trace.ticks.size() == 61);
  CHECK(traces[1].trace.section_names == std::vector<std::string>{"state"});
  CHECK(findTraceDivergence(traces)->tick == 37);
}

TEST_CASE("a server that reports its own hashes has a trace of its own") {
  LoopbackSession session(keeping(1));
  LockstepClient& a = session.join();
  session.pump();
  (void)session.start("arena");
  for (uint64_t tick = 0; tick <= 60; ++tick) {
    session.server().reportHash(hashOf(tick, 1));
  }
  a.reportHash(hashOf(0, 1));
  a.reportHash(hashOf(60, 5));
  session.pump();
  const auto traces = session.server().traces();
  REQUIRE(traces.size() == 2);
  CHECK(traces[1].slot == NET_SERVER_SLOT);
  CHECK(traces[1].trace.ticks.size() == 61);
}

TEST_CASE("a server refuses a client built from other code, or without the "
          "password") {
  LockstepServerConfig config;
  config.build = 7;
  config.password = netPasswordDigest("hunter2");
  LoopbackSession session(config);
  const uint64_t right = netPasswordDigest("hunter2");
  LockstepClient& other = session.join({NET_PROTOCOL_VERSION, 0, 8, right, ""});
  LockstepClient& guess =
      session.join({NET_PROTOCOL_VERSION, 0, 7, netPasswordDigest("x"), ""});
  LockstepClient& open = session.join({NET_PROTOCOL_VERSION, 0, 7, 0, ""});
  LockstepClient& known = session.join({NET_PROTOCOL_VERSION, 0, 7, right, ""});
  session.pump();
  CHECK(other.refusal() == NetRefusalReason::BUILD);
  CHECK(guess.refusal() == NetRefusalReason::PASSWORD);
  CHECK(open.refusal() == NetRefusalReason::PASSWORD);
  CHECK(known.slot() == 0);
}

TEST_CASE("a seat removed mid-run is told why, and plays absent") {
  LoopbackSession session(keeping(1));
  session.seat(2);
  (void)session.start("arena");
  session.server().removeSeat(1, NetRefusalReason::STALLED);
  session.pump();
  CHECK(session.client(1).state() == NetClientState::REFUSED);
  CHECK(session.client(1).refusal() == NetRefusalReason::STALLED);
  CHECK(session.server().playing() == 0b01);
  (void)drain(session.client(0));
  (void)session.client(0).sendInput({});
  session.pump();
  CHECK(session.client(0).takeFrame()->absent == 0b10);
}
