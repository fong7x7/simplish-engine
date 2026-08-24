#pragma once

// Design Summary -- Steam Networking
// Technical Approach:
// docs/technical-approaches/engine/platform-steam/networking.md
//
// Behaviours:
//   - Create relay connection to a remote peer via Valve relay network
//   - Create direct UDP connection to an IP:port via Steam Networking Sockets
//   - Listen for incoming connections (relay or direct)
//   - Send messages with configurable reliability
//   - Receive pending messages from a connection
//   - Close connections and listeners gracefully
//   - Query connection status
//
// Edge Cases:
//   - Steam not available: connection creation returns NOT_AVAILABLE
//   - Relay server unreachable: connection transitions to PROBLEM_DETECTED
//   - Send on closed connection: returns SEND_FAILED
//   - Receive with no pending messages: returns empty vector
//   - Close listener with active connections: connections closed first
//
// Invariants:
//   - Steam Networking Sockets is optional; ENet is primary default
//   - Transport negotiated per-connection; both peers must support chosen mode
//   - Message data uses std::span<const std::byte> / std::vector<std::byte>
//   - Main thread only
//
// Integration Points:
//   - Engine Networking: Steam transport is one of multiple backends
//   - Matchmaking: lobby game server provides connection details

#include "steam-types.h"

#include <cstddef>
#include <cstdint>
#include <engine/core/expected-polyfill.h>
#include <span>
#include <string_view>
#include <vector>

namespace eng {

// Forward declaration
struct SteamContext;

// ---------------------------------------------------------------------------
// Public API
// ---------------------------------------------------------------------------

/// Connect to a remote Steam user via Valve's relay network.
/// Returns a connection handle on success. The connection starts in
/// CONNECTING state; poll with queryConnectionStatus().
/// Main thread only.
std::expected<SteamConnectionHandle, SteamError>
connectRelay(const SteamContext& ctx, SteamUserId remote_user,
             uint16_t virtual_port);

/// Connect directly to an IP:port via Steam Networking Sockets
/// (no relay, but with Steam authentication and encryption).
/// Main thread only.
std::expected<SteamConnectionHandle, SteamError>
connectDirect(const SteamContext& ctx, std::string_view ip, uint16_t port);

/// Listen for incoming relay connections on a virtual port.
/// Incoming connections are auto-accepted and available via receiveMessages().
/// Main thread only.
std::expected<SteamListenerHandle, SteamError>
listenRelay(const SteamContext& ctx, uint16_t virtual_port);

/// Listen for incoming direct connections on a local port.
/// Main thread only.
std::expected<SteamListenerHandle, SteamError>
listenDirect(const SteamContext& ctx, uint16_t port);

/// Send a message on an established connection. The data is copied
/// internally by the SDK. Returns true on success.
/// Main thread only.
std::expected<bool, SteamError> sendMessage(const SteamContext& ctx,
                                            SteamConnectionHandle conn,
                                            std::span<const std::byte> data,
                                            SteamSendFlags flags);

/// Receive up to max_messages pending messages from a connection.
/// Each message is returned as an owned byte buffer. Returns empty
/// vector if no messages are pending (not an error).
/// Main thread only.
std::vector<std::vector<std::byte>> receiveMessages(const SteamContext& ctx,
                                                    SteamConnectionHandle conn,
                                                    uint32_t max_messages);

/// Close a connection gracefully. Idempotent.
/// Main thread only.
void closeConnection(const SteamContext& ctx, SteamConnectionHandle conn);

/// Close a listener. Active connections accepted via this listener
/// are closed first. Idempotent.
/// Main thread only.
void closeListener(const SteamContext& ctx, SteamListenerHandle listener);

/// Query the current status of a connection.
/// Main thread only.
SteamConnectionStatus queryConnectionStatus(const SteamContext& ctx,
                                            SteamConnectionHandle conn);

}  // namespace eng
