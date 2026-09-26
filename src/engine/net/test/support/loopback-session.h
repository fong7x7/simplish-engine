#pragma once

/// @file loopback-session.h
/// @brief A server and its clients on one loopback network, for tests.
/// @par Threading
/// Main-thread-only.

#include <cstddef>
#include <engine/net/lockstep-client.h>
#include <engine/net/lockstep-server.h>
#include <engine/net/loopback-network.h>
#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace eng::net::test {

/// A lockstep server and however many clients a test joins to it, all in
/// one process. `pump` polls everyone until the messages settle.
class LoopbackSession {
public:
  /// A session whose server is configured by @p config.
  explicit LoopbackSession(const LockstepServerConfig& config = {});

  /// A new client asking for a seat with @p hello. Not yet connected:
  /// `pump` to let it in.
  LockstepClient& join(const NetHello& hello = {});

  /// Join @p count clients with default hellos, and pump until seated.
  void seat(std::size_t count);

  /// Disconnect client @p index, destroying it.
  void leave(std::size_t index);

  /// Start a run of @p level with @p seed, and pump so every client has
  /// heard it. Nothing when nobody is seated.
  std::optional<NetStart> start(const std::string& level, uint64_t seed = 0);

  /// Poll the server and every client, round after round, until nothing
  /// new has had time to arrive.
  void pump();

  /// The server.
  [[nodiscard]] LockstepServer& server() { return *server_; }

  /// The network, to pretend it is slower than it is.
  [[nodiscard]] LoopbackNetwork& network() { return network_; }

  /// Client @p index, in joining order.
  [[nodiscard]] LockstepClient& client(std::size_t index) {
    return *clients_[index];
  }

private:
  /// The network everything is on.
  LoopbackNetwork network_;
  /// The server.
  std::unique_ptr<LockstepServer> server_;
  /// Every client joined, null once it has left.
  std::vector<std::unique_ptr<LockstepClient>> clients_;
};

}  // namespace eng::net::test
