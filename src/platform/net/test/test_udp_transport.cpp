#include <array>
#include <catch2/catch_test_macros.hpp>
#include <chrono>
#include <enet/enet.h>
#include <engine/net/lockstep-client.h>
#include <engine/net/lockstep-server.h>
#include <engine/net/net-codec.h>
#include <engine/net/udp-connect.h>
#include <engine/net/udp-listen.h>
#include <functional>
#include <memory>
#include <optional>
#include <thread>
#include <vector>

using namespace eng;
using namespace eng::net;

namespace {

/// Poll @p step until it says it is done, for at most @p limit of real
/// time: these tests use real sockets on the loopback interface.
bool within(const std::function<bool()>& step,
            std::chrono::milliseconds limit = std::chrono::seconds(5)) {
  const auto deadline = std::chrono::steady_clock::now() + limit;
  while (std::chrono::steady_clock::now() < deadline) {
    if (step()) {
      return true;
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(1));
  }
  return false;
}

/// The next event of @p transport, waiting for it.
std::optional<NetEvent> next(NetTransport& transport) {
  std::optional<NetEvent> event;
  (void)within([&] {
    event = transport.poll();
    return event.has_value();
  });
  return event;
}

/// A client connected to @p server's listener on @p port, with the
/// peer each side knows the other by.
struct Connected {
  /// The client's transport.
  std::unique_ptr<NetTransport> client;
  /// The client, as the server numbers it.
  NetPeer at_server = 0;
  /// The server, as the client numbers it.
  NetPeer at_client = 0;
};

/// Connect a client to @p server on @p port, polling both ends — ENet's
/// handshake needs each side's replies — until both have heard it.
std::optional<Connected> connectTo(NetTransport& server, uint16_t port) {
  Connected link{connectUdp("127.0.0.1", port), 0, 0};
  std::optional<NetEvent> joined;
  std::optional<NetEvent> connected;
  const bool done = link.client && within([&] {
                      std::optional<NetEvent> from_server = server.poll();
                      std::optional<NetEvent> from_client = link.client->poll();
                      joined = joined ? joined : from_server;
                      connected = connected ? connected : from_client;
                      return joined && connected;
                    });
  if (!done || joined->kind != NetEventKind::CONNECTED) {
    return std::nullopt;
  }
  link.at_server = joined->peer;
  link.at_client = connected->peer;
  return link;
}

/// Poll @p server and both clients until each client has taken 120
/// frames.
bool play120(LockstepServer& server, LockstepClient& a, LockstepClient& b) {
  std::array<uint64_t, 2> ticks{};
  return within([&] {
    server.poll();
    for (std::size_t i = 0; i < 2; ++i) {
      LockstepClient& client = i == 0 ? a : b;
      client.poll();
      while (client.takeFrame()) {
        ++ticks[i];
      }
      (void)client.sendInput({});
    }
    return ticks[0] >= 120 && ticks[1] >= 120;
  });
}

/// A raw ENet client with no message cap of its own, as a hostile peer's
/// would be, connecting to 127.0.0.1 on a port.
class RawClient {
public:
  explicit RawClient(uint16_t port)
    : host_(enet_host_create(nullptr, 1, 1, 0, 0)) {
    ENetAddress address{0, port};
    (void)enet_address_set_host(&address, "127.0.0.1");
    peer_ = enet_host_connect(host_, &address, 1, 0);
  }
  ~RawClient() { enet_host_destroy(host_); }
  RawClient(const RawClient&) = delete;
  RawClient& operator=(const RawClient&) = delete;
  RawClient(RawClient&&) = delete;
  RawClient& operator=(RawClient&&) = delete;

  /// Service the host; whether that finished connecting.
  bool connected() {
    ENetEvent event;
    return enet_host_service(host_, &event, 0) > 0 &&
           event.type == ENET_EVENT_TYPE_CONNECT;
  }

  /// Service the host, whatever comes.
  void service() {
    ENetEvent event;
    (void)enet_host_service(host_, &event, 0);
  }

  /// Send @p bytes reliably, however many.
  void send(const std::vector<std::byte>& bytes) {
    (void)enet_peer_send(peer_, 0,
                         enet_packet_create(bytes.data(), bytes.size(),
                                            ENET_PACKET_FLAG_RELIABLE));
  }

private:
  /// The ENet host. Never null in a test that passed its setup.
  ENetHost* host_;
  /// The connection to the listener.
  ENetPeer* peer_ = nullptr;
};

}  // namespace

TEST_CASE("a UDP listener picks a port when asked for none") {
  const auto listen = listenUdp(0, 4);
  REQUIRE(listen);
  CHECK(listen->port != 0);
}

TEST_CASE("a UDP client that cannot resolve its server has no transport") {
  CHECK(connectUdp("no-such-host.invalid", 1) == nullptr);
}

TEST_CASE("UDP messages arrive whole and in order, each way") {
  auto listen = listenUdp(0, 4);
  REQUIRE(listen);
  NetTransport& server = *listen->transport;
  auto link = connectTo(server, listen->port);
  REQUIRE(link);
  const std::vector<std::byte> big(5000, std::byte{7});
  link->client->send(link->at_client, std::vector{std::byte{1}});
  link->client->send(link->at_client, big);
  CHECK(next(server)->bytes == std::vector{std::byte{1}});
  CHECK(next(server)->bytes == big);
  server.send(link->at_server, std::vector{std::byte{9}});
  CHECK(next(*link->client)->bytes == std::vector{std::byte{9}});
}

TEST_CASE("a UDP client that goes is heard going") {
  auto listen = listenUdp(0, 4);
  REQUIRE(listen);
  auto link = connectTo(*listen->transport, listen->port);
  REQUIRE(link);
  link->client.reset();
  const auto gone = next(*listen->transport);
  REQUIRE(gone);
  CHECK(gone->kind == NetEventKind::DISCONNECTED);
  CHECK(gone->peer == link->at_server);
}

TEST_CASE("a lockstep session runs over UDP") {
  auto listen = listenUdp(0, 4);
  REQUIRE(listen);
  const uint16_t port = listen->port;
  LockstepServer server(std::move(listen->transport), {});
  LockstepClient a(connectUdp("127.0.0.1", port), {});
  LockstepClient b(connectUdp("127.0.0.1", port), {});
  REQUIRE(within([&] {
    server.poll();
    a.poll();
    b.poll();
    return a.slot() && b.slot();
  }));
  REQUIRE(server.start("arena", 1));
  REQUIRE(play120(server, a, b));
  CHECK(server.nextTick() >= 120);
}

TEST_CASE("a UDP listener drops a message larger than the protocol's "
          "largest") {
  auto listen = listenUdp(0, 4);
  REQUIRE(listen);
  RawClient raw(listen->port);
  REQUIRE(within([&] {
    (void)listen->transport->poll();
    return raw.connected();
  }));
  raw.send(std::vector<std::byte>(NET_MAX_MESSAGE_BYTES + 1, std::byte{1}));
  const bool received = within(
      [&] {
        raw.service();
        const std::optional<NetEvent> event = listen->transport->poll();
        return event && event->kind == NetEventKind::RECEIVED;
      },
      std::chrono::seconds(1));
  CHECK_FALSE(received);
}
