#pragma once

/// @file deployed-server.h
/// @brief A deployed game's server: seats a session, runs one run of it.
/// @par Threading Main-thread-only.

#include "deployed-net-world.h"

#include <cstdint>
#include <deque>
#include <editor/deploy/deployed-game-options.h>
#include <editor/deploy/deployed-game-run.h>
#include <engine/net/lockstep-server.h>
#include <engine/net/net-transport.h>
#include <engine/sim/tick-hash.h>
#include <engine/sim/tick-result.h>
#include <game/logic/game-logic-factory.h>
#include <memory>
#include <ostream>

namespace eng::editor {

/// Where a deployed server's one run stands.
enum class DeployedServerPhase : uint8_t {
  WAITING,   ///< Seating players until there are enough
  RUNNING,   ///< The run is on
  FINISHED,  ///< The run is over, or could not start
};

/// The server of `simplish-game --serve` and `--host` (ADR-013): waits for
/// `players` clients, starts the options' level for them, and ends the run
/// after `max_ticks`, when it is over, or when everyone has left.
///
/// Serving, it also simulates the run itself from the frames it sends, as
/// the reference every client's hash is checked against, and ends the run
/// when its own world says it is over. Hosting, its own player's client
/// simulates instead, and whoever drives the host calls `endRun` when that
/// client's world is over.
/// @thread_safety Main-thread-only.
class DeployedServer {
public:
  /// A server on @p transport for the game and session @p options
  /// describe, running the logic @p logic makes.
  DeployedServer(std::unique_ptr<net::NetTransport> transport,
                 const DeployedGameOptions& options,
                 game::GameLogicFactory logic);

  /// Handle the network, start the run once enough are seated, and step
  /// the reference world on whatever frames have gone out.
  void poll(std::ostream& out);

  /// End the run for everyone.
  void endRun();

  /// Whether the run is over, or could not start.
  [[nodiscard]] bool finished() const {
    return phase_ == DeployedServerPhase::FINISHED;
  }
  /// How the run went, as the reference world saw it when serving.
  [[nodiscard]] const DeployedGameRun& run() const { return run_; }
  /// The session.
  [[nodiscard]] const net::LockstepServer& session() const { return server_; }

private:
  /// Start the run when enough players are seated.
  void startWhenSeated(std::ostream& out);
  /// Begin the run @p start describes.
  void begin(const net::NetStart& start, std::ostream& out);
  /// Keep up with the run: step the reference, or end a relay at its last
  /// frame; end it if it desynced or emptied.
  void follow(std::ostream& out);
  /// Step the reference world on every frame sent, and report its hashes.
  void stepReference(std::ostream& out);
  /// Report @p result's hash and say what its tick's logic said.
  void keep(const sim::TickResult& result, std::ostream& out);

  /// What the server was asked to run.
  DeployedGameOptions options_;
  /// What makes the logic's instances.
  game::GameLogicFactory logic_;
  /// The session.
  net::LockstepServer server_;
  /// The reference world, when serving.
  std::unique_ptr<DeployedNetWorld> world_;
  /// The reference world's recent checkpoint hashes.
  std::deque<sim::TickHash> checkpoints_;
  /// How the run went.
  DeployedGameRun run_;
  /// Where the run stands.
  DeployedServerPhase phase_ = DeployedServerPhase::WAITING;
};

}  // namespace eng::editor
