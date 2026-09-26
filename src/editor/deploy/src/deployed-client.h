#pragma once

/// @file deployed-client.h
/// @brief A deployed game's player in someone's co-op session.
/// @par Threading Main-thread-only.

#include "deployed-net-world.h"

#include <cstdint>
#include <deque>
#include <editor/deploy/deployed-game-options.h>
#include <editor/deploy/deployed-game-run.h>
#include <engine/net/lockstep-client.h>
#include <engine/net/net-transport.h>
#include <engine/sim/tick-hash.h>
#include <engine/sim/tick-result.h>
#include <game/logic/game-logic-factory.h>
#include <memory>
#include <ostream>

namespace eng::editor {

/// The client of `simplish-game --join` and `--host` (ADR-013): takes a
/// seat, builds the world of each run the server starts, steps it on the
/// server's frames, reports its hashes, and plays its own seat with a
/// stand-in — nobody holds the controls of a headless game. Finished when
/// the run is over and the server has ended it, or when it cannot go on.
/// @thread_safety Main-thread-only.
class DeployedClient {
public:
  /// A client on @p transport for the game @p options names, running the
  /// logic @p logic makes.
  DeployedClient(std::unique_ptr<net::NetTransport> transport,
                 const DeployedGameOptions& options,
                 game::GameLogicFactory logic);

  /// Handle the network, step every frame that has come, and send up to
  /// @p due inputs.
  void poll(uint32_t due, std::ostream& out);

  /// Whether the run's world has ended, as far as this client has stepped.
  [[nodiscard]] bool worldOver() const;
  /// Whether this client is done: its run is over and ended, or it cannot
  /// go on — the run's `error` says why.
  [[nodiscard]] bool finished() const { return finished_ != 0; }
  /// How the run went.
  [[nodiscard]] const DeployedGameRun& run() const { return run_; }
  /// The session.
  [[nodiscard]] const net::LockstepClient& session() const { return client_; }

private:
  /// Build the world of the run @p start begins.
  void begin(const net::NetStart& start, std::ostream& out);
  /// Step every frame that has come, while the world goes on.
  void stepFrames(std::ostream& out);
  /// Report @p result's hash and say what its tick's logic said.
  void keep(const sim::TickResult& result, std::ostream& out);
  /// Send up to @p due of this seat's stand-in's inputs.
  void sendInputs(uint32_t due);
  /// Decide whether the client is done.
  void checkFinished();
  /// Why the session cannot go on, or empty when it can, or ended well.
  [[nodiscard]] std::string failure() const;
  /// Be done, with the world's last state in the run.
  void finish();

  /// What the client was asked to run.
  DeployedGameOptions options_;
  /// What makes the logic's instances.
  game::GameLogicFactory logic_;
  /// The session.
  net::LockstepClient client_;
  /// The current run's world, once a run has started.
  std::unique_ptr<DeployedNetWorld> world_;
  /// Recent checkpoint hashes, to name a desync's section by.
  std::deque<sim::TickHash> checkpoints_;
  /// How the run went.
  DeployedGameRun run_;
  /// 1 once done.
  uint8_t finished_ = 0;
};

}  // namespace eng::editor
