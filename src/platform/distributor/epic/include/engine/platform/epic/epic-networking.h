#pragma once

// Design Summary -- Epic Networking (P2P Relay)
// Technical Approach:
//   docs/technical-approaches/engine/platform-epic/networking.md
//
// Behaviours:
//   - Open EOS P2P connection to remote peer by PUID
//   - Send/receive packets with channel and reliability mode
//   - Close connections
//   - Query connection status
//   - Multiple channels with configurable reliability
//
// Edge Cases:
//   - EOS not available: connection returns error; fallback to ENet
//   - Relay unreachable: connection transitions to closed
//   - Send on closed connection: returns error
//   - Receive with no pending packets: returns empty vector
//
// Invariants:
//   - EOS P2P is optional transport; ENet is primary default
//   - All networking functions main-thread-only
//   - No EOS SDK types in public API
//   - Cross-store play requires ENGINE_ENABLE_EPIC on all peers
//
// Integration Points:
//   - Engine Networking: Epic P2P is one of multiple transport backends
//   - Matchmaking: lobby members use P2P relay

#include "epic-packet-params.h"
#include "epic-types.h"

#include <cstddef>
#include <cstdint>
#include <engine/core/expected-polyfill.h>
#include <span>
#include <string>
#include <vector>

namespace eng {

// Forward declarations
struct EpicContext;

// ---------------------------------------------------------------------------
// Configuration
// ---------------------------------------------------------------------------

struct EpicP2PConfig {
  /// Logical socket identifier (e.g. "game").
  std::string socket_name{};
};

// ---------------------------------------------------------------------------
// Public API
// ---------------------------------------------------------------------------

/// Open an EOS P2P connection to a remote peer. Returns a
/// connection handle or error. Main thread only.
std::expected<EpicConnectionHandle, EpicError>
epicConnectP2P(const EpicContext& ctx, EpicProductUserId remote_user,
               const EpicP2PConfig& config);

/// Send a packet on an established P2P connection. Returns true on
/// success or error. Main thread only.
std::expected<bool, EpicError> epicSendPacket(const EpicContext& ctx,
                                              EpicConnectionHandle conn,
                                              const EpicPacketParams& packet);

/// Receive pending packets from a P2P connection. Returns up to
/// max_packets packets. Returns empty vector if no packets pending.
/// Main thread only.
std::vector<std::vector<std::byte>>
epicReceivePackets(const EpicContext& ctx, EpicConnectionHandle conn,
                   uint32_t max_packets);

/// Close a P2P connection. Safe to call on already-closed or
/// invalid handles. Main thread only.
void epicCloseP2P(const EpicContext& ctx, EpicConnectionHandle conn);

/// Query the status of a P2P connection. Main thread only.
EpicConnectionStatus epicQueryP2PStatus(const EpicContext& ctx,
                                        EpicConnectionHandle conn);

}  // namespace eng
