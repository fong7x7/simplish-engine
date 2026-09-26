#pragma once

/// @file deployed-server.h
/// @brief A deployed game's server: seats a session, runs one run of it.
/// @par Threading Main-thread-only.

#include "deployed-net-world.h"

#include <chrono>
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
#include <optional>
#include <ostream>

namespace eng::editor {

/// How long a run waits on someone before the server says who.
inline constexpr std::chrono::milliseconds DEPLOYED_STALL_NOTICE{250};

/// How long a server waits for every peer's trace after a desync before
/// it reports with the traces it has.
inline constexpr std::chrono::seconds DEPLOYED_TRACE_WAIT{2};

/// Where a deployed server's one run stands.
enum class DeployedServerPhase : uint8_t {
  WAITING,     ///< Seating players until there are enough
  RUNNING,     ///< The run is on
  COLLECTING,  ///< The run desynced; gathering the peers' traces
  FINISHED,    ///< The run is over, or could not start
};

/// The server of `simplish-game --serve` and `--host` (ADR-013): waits for
/// `players` clients, starts the options' level for them, and ends the run
/// after `max_ticks`, when it is over, or when everyone has left.
///
/// Serving, it also simulates the run itself from the frames it sends, as
/// the reference every client's hash is checked against — recording it,
/// when asked — and ends the run when its own world says it is over.
/// Hosting, its own player's client simulates instead, and whoever drives
/// the host calls `endRun` when that client's world is over.
///
/// It reads the wall clock for two things only, neither of which reaches
/// a tick: when a stall has gone on long enough to say who it is waiting
/// on, and how long to wait for traces after a desync.
/// @thread_safety Main-thread-only.
class DeployedServer {
public:
  /// A server on @p transport for the game and session @p options
  /// describe, running the logic @p logic makes.
  DeployedServer(std::unique_ptr<net::NetTransport> transport,
                 const DeployedGameOptions& options,
                 game::GameLogicFactory logic);

  /// Handle the network, start the run once enough are seated, step the
  /// reference world on whatever frames have gone out, say who a stall is
  /// waiting on, and report a desync once the traces are in.
  void poll(std::ostream& out);

  /// End the run for everyone, writing the reference replay if asked.
  void endRun(std::ostream& out);

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
  /// frame; notice a stall, a desync, or everyone gone.
  void follow(std::ostream& out);
  /// Step the reference world on every frame sent, and report its hashes.
  void stepReference(std::ostream& out);
  /// Report @p result's hash and say what its tick's logic said.
  void keep(const sim::TickResult& result, std::ostream& out);
  /// Say who the run is waiting on, once it has waited long enough.
  void noticeStall(std::ostream& out);
  /// Report the desync once every trace is in, or the wait is over.
  void collectTraces(std::ostream& out);
  /// Write the desync's report, and say where in the run's error.
  void reportDesync();

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
  /// The next frame's tick when the server last saw it move.
  uint64_t progress_tick_ = 0;
  /// When that was.
  std::chrono::steady_clock::time_point progress_at_{};
  /// The tick whose wait was last announced.
  std::optional<uint64_t> announced_tick_;
  /// When the desync being reported was noticed.
  std::chrono::steady_clock::time_point desynced_at_{};
};

}  // namespace eng::editor
