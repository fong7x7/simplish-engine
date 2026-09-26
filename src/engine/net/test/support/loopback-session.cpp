#include "support/loopback-session.h"

namespace eng::net::test {

namespace {

  /// Rounds of polling that settle any exchange the tests make: a hello,
  /// its welcome and a roster, then a start and its frames.
  constexpr int PUMP_ROUNDS = 6;

}  // namespace

LoopbackSession::LoopbackSession(const LockstepServerConfig& config)
  : server_(std::make_unique<LockstepServer>(network_.listen(), config)) {}

LockstepClient& LoopbackSession::join(const NetHello& hello) {
  clients_.push_back(
      std::make_unique<LockstepClient>(network_.connect(), hello));
  return *clients_.back();
}

void LoopbackSession::leave(std::size_t index) {
  clients_[index].reset();
}

std::optional<NetStart> LoopbackSession::start(const std::string& level,
                                               uint64_t seed) {
  std::optional<NetStart> started = server_->start(level, seed);
  pump();
  return started;
}

void LoopbackSession::pump() {
  for (int round = 0; round < PUMP_ROUNDS; ++round) {
    server_->poll();
    for (const std::unique_ptr<LockstepClient>& client : clients_) {
      if (client) {
        client->poll();
      }
    }
  }
}

}  // namespace eng::net::test
