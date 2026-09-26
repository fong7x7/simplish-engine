#include "loopback-transport.h"

#include <engine/net/loopback-network.h>
#include <utility>

namespace eng::net {

namespace {

  /// Put @p kind for @p peer in endpoint @p to's inbox.
  void post(LoopbackHub& hub, NetPeer to, NetEventKind kind, NetPeer peer) {
    hub.inboxes[to].push_back({kind, peer, {}});
  }

  /// Close the connection of client endpoint @p client, telling both ends.
  void close(LoopbackHub& hub, NetPeer client) {
    if (hub.open[client] == 0) {
      return;
    }
    hub.open[client] = 0;
    post(hub, client, NetEventKind::DISCONNECTED, LOOPBACK_SERVER_PEER);
    post(hub, LOOPBACK_SERVER_PEER, NetEventKind::DISCONNECTED, client);
  }

}  // namespace

LoopbackTransport::LoopbackTransport(std::shared_ptr<LoopbackHub> hub,
                                     NetPeer endpoint)
  : hub_(std::move(hub)), endpoint_(endpoint) {}

// Exceptions are off (ADR-001): a failed allocation here aborts instead.
// NOLINTNEXTLINE(bugprone-exception-escape)
LoopbackTransport::~LoopbackTransport() {
  if (endpoint_ != LOOPBACK_SERVER_PEER) {
    close(*hub_, endpoint_);
    return;
  }
  hub_->listening = 0;
  for (NetPeer client = 1; client < hub_->open.size(); ++client) {
    close(*hub_, client);
  }
}

std::optional<NetEvent> LoopbackTransport::poll() {
  std::deque<NetEvent>& inbox = hub_->inboxes[endpoint_];
  if (inbox.empty()) {
    return std::nullopt;
  }
  NetEvent event = std::move(inbox.front());
  inbox.pop_front();
  return event;
}

NetPeer LoopbackTransport::clientEnd(NetPeer peer) const {
  return endpoint_ == LOOPBACK_SERVER_PEER ? peer : endpoint_;
}

void LoopbackTransport::send(NetPeer peer, std::span<const std::byte> bytes) {
  const NetPeer client = clientEnd(peer);
  if (client >= hub_->open.size() || hub_->open[client] == 0) {
    return;
  }
  const NetPeer to =
      endpoint_ == LOOPBACK_SERVER_PEER ? client : LOOPBACK_SERVER_PEER;
  const NetPeer from =
      endpoint_ == LOOPBACK_SERVER_PEER ? LOOPBACK_SERVER_PEER : client;
  hub_->inboxes[to].push_back(
      {NetEventKind::RECEIVED, from, {bytes.begin(), bytes.end()}});
}

void LoopbackTransport::disconnect(NetPeer peer) {
  const NetPeer client = clientEnd(peer);
  if (client < hub_->open.size()) {
    close(*hub_, client);
  }
}

std::optional<uint32_t> LoopbackTransport::roundTripMs(NetPeer peer) const {
  const NetPeer client = clientEnd(peer);
  if (client >= hub_->open.size() || hub_->open[client] == 0) {
    return std::nullopt;
  }
  return hub_->round_trip_ms;
}

std::unique_ptr<NetTransport> LoopbackNetwork::listen() {
  if (hub_->listening != 0) {
    return nullptr;
  }
  hub_->listening = 1;
  return std::make_unique<LoopbackTransport>(hub_, LOOPBACK_SERVER_PEER);
}

std::unique_ptr<NetTransport> LoopbackNetwork::connect() {
  const auto client = static_cast<NetPeer>(hub_->inboxes.size());
  hub_->inboxes.emplace_back();
  hub_->open.push_back(hub_->listening);
  if (hub_->listening == 0) {
    post(*hub_, client, NetEventKind::DISCONNECTED, LOOPBACK_SERVER_PEER);
  } else {
    post(*hub_, client, NetEventKind::CONNECTED, LOOPBACK_SERVER_PEER);
    post(*hub_, LOOPBACK_SERVER_PEER, NetEventKind::CONNECTED, client);
  }
  return std::make_unique<LoopbackTransport>(hub_, client);
}

}  // namespace eng::net
