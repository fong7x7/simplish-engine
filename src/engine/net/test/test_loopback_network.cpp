#include <catch2/catch_test_macros.hpp>
#include <engine/net/loopback-network.h>
#include <vector>

using namespace eng::net;

namespace {

std::vector<std::byte> bytesOf(std::initializer_list<uint8_t> values) {
  std::vector<std::byte> bytes;
  for (const uint8_t value : values) {
    bytes.push_back(std::byte{value});
  }
  return bytes;
}

}  // namespace

TEST_CASE("a loopback client and its listener both hear it connect") {
  LoopbackNetwork network;
  const auto server = network.listen();
  const auto client = network.connect();
  const auto server_event = server->poll();
  REQUIRE(server_event);
  CHECK(server_event->kind == NetEventKind::CONNECTED);
  const auto client_event = client->poll();
  REQUIRE(client_event);
  CHECK(client_event->kind == NetEventKind::CONNECTED);
  CHECK(client_event->peer == LOOPBACK_SERVER_PEER);
  CHECK_FALSE(server->poll());
}

TEST_CASE("a loopback network has one listener, and nothing to connect to "
          "without it") {
  LoopbackNetwork network;
  const auto client = network.connect();
  CHECK(client->poll()->kind == NetEventKind::DISCONNECTED);
  const auto server = network.listen();
  CHECK(server != nullptr);
  CHECK(network.listen() == nullptr);
}

TEST_CASE("loopback messages arrive whole and in order, each way") {
  LoopbackNetwork network;
  const auto server = network.listen();
  const auto client = network.connect();
  const NetPeer peer = server->poll()->peer;
  (void)client->poll();
  client->send(LOOPBACK_SERVER_PEER, bytesOf({1, 2}));
  client->send(LOOPBACK_SERVER_PEER, bytesOf({3}));
  server->send(peer, bytesOf({9}));
  const auto first = server->poll();
  REQUIRE(first);
  CHECK(first->kind == NetEventKind::RECEIVED);
  CHECK(first->peer == peer);
  CHECK(first->bytes == bytesOf({1, 2}));
  CHECK(server->poll()->bytes == bytesOf({3}));
  CHECK(client->poll()->bytes == bytesOf({9}));
}

TEST_CASE("a loopback disconnect is heard on both sides, and ends sends") {
  LoopbackNetwork network;
  const auto server = network.listen();
  const auto client = network.connect();
  const NetPeer peer = server->poll()->peer;
  (void)client->poll();
  server->disconnect(peer);
  CHECK(client->poll()->kind == NetEventKind::DISCONNECTED);
  CHECK(server->poll()->kind == NetEventKind::DISCONNECTED);
  client->send(LOOPBACK_SERVER_PEER, bytesOf({1}));
  CHECK_FALSE(server->poll());
}

TEST_CASE("destroying a loopback endpoint disconnects the other side") {
  LoopbackNetwork network;
  auto server = network.listen();
  auto client = network.connect();
  (void)server->poll();
  (void)client->poll();
  client.reset();
  CHECK(server->poll()->kind == NetEventKind::DISCONNECTED);
  auto other = network.connect();
  (void)other->poll();
  server.reset();
  CHECK(other->poll()->kind == NetEventKind::DISCONNECTED);
}
