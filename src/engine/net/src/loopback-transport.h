#pragma once

/// @file loopback-transport.h
/// @brief One endpoint of a `LoopbackNetwork`.
/// @par Threading
/// Main-thread-only.

#include <cstddef>
#include <engine/net/loopback-hub.h>
#include <engine/net/net-transport.h>
#include <memory>
#include <optional>
#include <span>

namespace eng::net {

/// A transport whose sends are pushes onto the receiver's inbox in the
/// shared hub. Endpoint 0 is the listener; any other is a client, which
/// knows the listener as `LOOPBACK_SERVER_PEER`.
class LoopbackTransport final : public NetTransport {
public:
  /// Endpoint @p endpoint of @p hub.
  LoopbackTransport(std::shared_ptr<LoopbackHub> hub, NetPeer endpoint);
  /// Closes every connection this endpoint has.
  ~LoopbackTransport() override;
  LoopbackTransport(const LoopbackTransport&) = delete;
  LoopbackTransport& operator=(const LoopbackTransport&) = delete;
  LoopbackTransport(LoopbackTransport&&) = delete;
  LoopbackTransport& operator=(LoopbackTransport&&) = delete;

  std::optional<NetEvent> poll() override;
  void send(NetPeer peer, std::span<const std::byte> bytes) override;
  void disconnect(NetPeer peer) override;
  [[nodiscard]] std::optional<uint32_t>
  roundTripMs(NetPeer peer) const override;

private:
  /// The client endpoint a connection to @p peer is, from this side.
  [[nodiscard]] NetPeer clientEnd(NetPeer peer) const;

  /// The shared queues.
  std::shared_ptr<LoopbackHub> hub_;
  /// This endpoint's number in the hub.
  NetPeer endpoint_;
};

}  // namespace eng::net
