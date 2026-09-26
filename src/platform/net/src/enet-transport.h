#pragma once

/// @file enet-transport.h
/// @brief `NetTransport` over an ENet host.
/// @par Threading
/// Main-thread-only.

#include <cstddef>
#include <enet/enet.h>
#include <engine/net/net-transport.h>
#include <optional>
#include <span>

namespace eng::net {

/// Seconds' worth of ENet's timeout bounds: a peer silent for between
/// these many milliseconds is disconnected, rather than ENet's default of
/// up to thirty seconds — a dropped player should be noticed quickly.
inline constexpr enet_uint32 ENET_DROP_MIN_MS = 2000;
/// See `ENET_DROP_MIN_MS`.
inline constexpr enet_uint32 ENET_DROP_MAX_MS = 8000;

/// A server's or a client's ENet host, as a transport. Peers are numbered
/// by their slot in the host (`incomingPeerID`); every message is a
/// reliable packet on channel 0, flushed as it is sent.
class EnetTransport final : public NetTransport {
public:
  /// A transport over @p host, which it owns and destroys.
  explicit EnetTransport(ENetHost* host);
  /// Tells every connected peer it is going, then destroys the host.
  ~EnetTransport() override;
  EnetTransport(const EnetTransport&) = delete;
  EnetTransport& operator=(const EnetTransport&) = delete;
  EnetTransport(EnetTransport&&) = delete;
  EnetTransport& operator=(EnetTransport&&) = delete;

  std::optional<NetEvent> poll() override;
  void send(NetPeer peer, std::span<const std::byte> bytes) override;
  void disconnect(NetPeer peer) override;

private:
  /// The ENet peer numbered @p peer, when it is connected.
  [[nodiscard]] ENetPeer* connected(NetPeer peer) const;

  /// The host. Never null.
  ENetHost* host_;
};

/// Whether ENet is ready to use: initialised on the first call, and torn
/// down at exit.
[[nodiscard]] bool enetReady();

}  // namespace eng::net
