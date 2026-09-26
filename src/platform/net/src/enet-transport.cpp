#include "enet-transport.h"

#include <cstdlib>
#include <engine/net/net-codec.h>
#include <engine/net/udp-connect.h>
#include <engine/net/udp-listen.h>

namespace eng::net {

namespace {

  /// @p event as a transport event: whose, and what, with any packet's
  /// bytes copied out and the packet freed.
  NetEvent toNetEvent(const ENetEvent& event) {
    const NetPeer peer = event.peer->incomingPeerID;
    if (event.type == ENET_EVENT_TYPE_CONNECT) {
      enet_peer_timeout(event.peer, 0, ENET_DROP_MIN_MS, ENET_DROP_MAX_MS);
      return {NetEventKind::CONNECTED, peer, {}};
    }
    if (event.type == ENET_EVENT_TYPE_DISCONNECT) {
      return {NetEventKind::DISCONNECTED, peer, {}};
    }
    // NOLINTNEXTLINE(cppcoreguidelines-pro-type-reinterpret-cast) bytes are
    // bytes
    const auto* data = reinterpret_cast<const std::byte*>(event.packet->data);
    NetEvent received{
        NetEventKind::RECEIVED, peer, {data, data + event.packet->dataLength}};
    enet_packet_destroy(event.packet);
    return received;
  }

}  // namespace

bool enetReady() {
  static const bool ready = [] {
    if (enet_initialize() != 0) {
      return false;
    }
    (void)std::atexit(enet_deinitialize);
    return true;
  }();
  return ready;
}

EnetTransport::EnetTransport(ENetHost* host) : host_(host) {
  // Nothing larger than the protocol's largest message is accepted, or
  // buffered while it arrives: a peer cannot make this host hold megabytes.
  host_->maximumPacketSize = NET_MAX_MESSAGE_BYTES;
  host_->maximumWaitingData = ENET_MAX_WAITING_BYTES;
}

EnetTransport::~EnetTransport() {
  for (size_t i = 0; i < host_->peerCount; ++i) {
    if (host_->peers[i].state == ENET_PEER_STATE_CONNECTED) {
      enet_peer_disconnect_now(&host_->peers[i], 0);
    }
  }
  enet_host_flush(host_);
  enet_host_destroy(host_);
}

std::optional<NetEvent> EnetTransport::poll() {
  ENetEvent event;
  if (enet_host_service(host_, &event, 0) <= 0) {
    return std::nullopt;
  }
  return toNetEvent(event);
}

ENetPeer* EnetTransport::connected(NetPeer peer) const {
  if (peer >= host_->peerCount ||
      host_->peers[peer].state != ENET_PEER_STATE_CONNECTED) {
    return nullptr;
  }
  return &host_->peers[peer];
}

void EnetTransport::send(NetPeer peer, std::span<const std::byte> bytes) {
  ENetPeer* to = connected(peer);
  if (to == nullptr) {
    return;
  }
  ENetPacket* packet =
      enet_packet_create(bytes.data(), bytes.size(), ENET_PACKET_FLAG_RELIABLE);
  if (enet_peer_send(to, 0, packet) != 0) {
    enet_packet_destroy(packet);
  }
  enet_host_flush(host_);
}

void EnetTransport::disconnect(NetPeer peer) {
  if (ENetPeer* to = connected(peer)) {
    enet_peer_disconnect_later(to, 0);
  }
}

std::optional<uint32_t> EnetTransport::roundTripMs(NetPeer peer) const {
  const ENetPeer* to = connected(peer);
  if (to == nullptr) {
    return std::nullopt;
  }
  return to->roundTripTime + to->roundTripTimeVariance;
}

std::optional<UdpListen> listenUdp(uint16_t port, uint8_t max_peers) {
  ENetAddress address{ENET_HOST_ANY, port};
  ENetHost* host =
      enetReady() ? enet_host_create(&address, max_peers, 1, 0, 0) : nullptr;
  if (host == nullptr) {
    return std::nullopt;
  }
  ENetAddress bound{};
  (void)enet_socket_get_address(host->socket, &bound);
  return UdpListen{std::make_unique<EnetTransport>(host), bound.port};
}

std::unique_ptr<NetTransport> connectUdp(const std::string& host,
                                         uint16_t port) {
  ENetAddress address{0, port};
  if (!enetReady() || enet_address_set_host(&address, host.c_str()) != 0) {
    return nullptr;
  }
  ENetHost* client = enet_host_create(nullptr, 1, 1, 0, 0);
  if (client == nullptr) {
    return nullptr;
  }
  auto transport = std::make_unique<EnetTransport>(client);
  // Failing to connect shows as a DISCONNECTED poll, like any other — and
  // as soon as a silent peer would, not after ENet's thirty seconds.
  if (ENetPeer* server = enet_host_connect(client, &address, 1, 0)) {
    enet_peer_timeout(server, 0, ENET_DROP_MIN_MS, ENET_DROP_MAX_MS);
  }
  return transport;
}

}  // namespace eng::net
