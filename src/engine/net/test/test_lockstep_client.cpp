#include "support/loopback-session.h"

#include <catch2/catch_test_macros.hpp>
#include <engine/net/lockstep-client.h>
#include <engine/net/loopback-network.h>
#include <utility>
#include <vector>

using namespace eng;
using namespace eng::net;
using eng::net::test::LoopbackSession;

namespace {

LockstepServerConfig delayed(uint8_t delay) {
  LockstepServerConfig config;
  config.input_delay = delay;
  return config;
}

/// Every frame @p client has, in order.
std::vector<NetFrame> takeAll(LockstepClient& client) {
  std::vector<NetFrame> frames;
  while (auto frame = client.takeFrame()) {
    frames.push_back(*frame);
  }
  return frames;
}

/// Play @p steps steps of the session's first two clients — each takes
/// its frames, then sends the step number on its own stick axis — and
/// give the frames each took.
std::pair<std::vector<NetFrame>, std::vector<NetFrame>>
playSteps(LoopbackSession& session, int16_t steps) {
  std::vector<NetFrame> seen_a;
  std::vector<NetFrame> seen_b;
  for (int16_t step = 0; step < steps; ++step) {
    for (const NetFrame& frame : takeAll(session.client(0))) {
      seen_a.push_back(frame);
    }
    for (const NetFrame& frame : takeAll(session.client(1))) {
      seen_b.push_back(frame);
    }
    (void)session.client(0).sendInput({step, 0, 0, 0, 0, 0});
    (void)session.client(1).sendInput({0, step, 0, 0, 0, 0});
    session.pump();
  }
  return {seen_a, seen_b};
}

}  // namespace

TEST_CASE("a client with nothing to connect to says so") {
  LoopbackNetwork network;
  LockstepClient client(network.connect(), {});
  client.poll();
  CHECK(client.state() == NetClientState::DISCONNECTED);
  CHECK_FALSE(client.sendInput({}));
}

TEST_CASE("a client sends nothing before a run starts") {
  LoopbackSession session;
  LockstepClient& client = session.join();
  session.pump();
  CHECK(client.state() == NetClientState::SEATED);
  CHECK_FALSE(client.sendInput({}));
  CHECK_FALSE(client.takeStart());
}

TEST_CASE("a client's input runs exactly the input delay ahead of its "
          "frames") {
  LoopbackSession session(delayed(3));
  LockstepClient& client = session.join();
  session.pump();
  (void)session.start("arena", 0);
  CHECK(client.nextInputTick() == 3);
  CHECK_FALSE(client.sendInput({}));  // Frames 0-2 not yet taken.
  CHECK(client.takeFrame()->tick == 0);
  CHECK(client.sendInput({}));
  CHECK_FALSE(client.sendInput({}));
  CHECK(client.takeFrame()->tick == 1);
  CHECK(client.takeFrame()->tick == 2);
  CHECK(client.sendInput({}));
  CHECK(client.sendInput({}));
  CHECK_FALSE(client.sendInput({}));
  CHECK(client.nextInputTick() == 6);
}

TEST_CASE("clients in one run receive the same frames in the same order") {
  LoopbackSession session(delayed(2));
  (void)session.join();
  (void)session.join();
  session.pump();
  (void)session.start("arena", 7);
  const auto [seen_a, seen_b] = playSteps(session, 50);
  REQUIRE(seen_a.size() >= 48);
  CHECK(seen_a == seen_b);
  for (std::size_t i = 0; i < seen_a.size(); ++i) {
    CHECK(seen_a[i].tick == i);
  }
  CHECK(seen_a[10].input.players[0].move_x == 8);
  CHECK(seen_a[10].input.players[1].move_y == 8);
}

TEST_CASE("a client keeps the frames sent before an end, and takes the end "
          "after them") {
  LoopbackSession session(delayed(2));
  LockstepClient& client = session.join();
  session.pump();
  (void)session.server().start("arena", 0);
  session.server().end();
  session.pump();
  CHECK(client.state() == NetClientState::SEATED);
  CHECK(client.framesWaiting() == 2);
}

TEST_CASE("a client whose server goes away is disconnected") {
  LoopbackNetwork network;
  auto server = std::make_unique<LockstepServer>(network.listen(),
                                                 LockstepServerConfig{});
  LockstepClient client(network.connect(), {});
  server->poll();
  client.poll();
  server->poll();
  client.poll();
  REQUIRE(client.state() == NetClientState::SEATED);
  server.reset();
  client.poll();
  CHECK(client.state() == NetClientState::DISCONNECTED);
}
