#include "deployed-server.h"

#include "deployed-build-id.h"
#include "deployed-level.h"
#include "deployed-replay.h"
#include "desync-report.h"
#include "desync-text.h"
#include "seats-text.h"

#include <bit>
#include <editor/deploy/deployed-content.h>
#include <engine/net/net-password.h>
#include <fstream>
#include <utility>

namespace eng::editor {

namespace {

  /// The session a server for @p options runs.
  net::LockstepServerConfig sessionFor(const DeployedGameOptions& options) {
    net::LockstepServerConfig config;
    config.content_hash = deployedContentHash(options.content);
    config.build = deployedBuildId();
    config.password = net::netPasswordDigest(options.password);
    config.input_delay =
        options.input_delay.value_or(net::NET_DEFAULT_INPUT_DELAY);
    config.delay_choice = options.input_delay ? net::NetDelayChoice::FIXED
                                              : net::NetDelayChoice::MEASURED;
    config.frames = options.mode == DeployedGameMode::SERVE
                        ? net::NetServerFrames::KEEP
                        : net::NetServerFrames::RELAY;
    return config;
  }

  /// @p text written to @p path. False when it could not be.
  bool writeText(const std::filesystem::path& path, const std::string& text) {
    std::ofstream file(path);
    file << text;
    return file.good();
  }

}  // namespace

DeployedServer::DeployedServer(std::unique_ptr<net::NetTransport> transport,
                               const DeployedGameOptions& options,
                               game::GameLogicFactory logic)
  : options_(options), logic_(logic),
    server_(std::move(transport), sessionFor(options)) {}

void DeployedServer::poll(std::ostream& out) {
  server_.poll();
  if (phase_ == DeployedServerPhase::WAITING) {
    startWhenSeated(out);
  } else if (phase_ == DeployedServerPhase::RUNNING) {
    follow(out);
  } else if (phase_ == DeployedServerPhase::COLLECTING) {
    collectTraces(out);
  }
}

void DeployedServer::startWhenSeated(std::ostream& out) {
  if (std::popcount(server_.seated()) < options_.players) {
    return;
  }
  const std::optional<game::GameSetup> setup = manifestSetup(options_, run_);
  const auto start =
      setup ? server_.start(run_.level, setup->seed) : std::nullopt;
  if (!start) {
    phase_ = DeployedServerPhase::FINISHED;
    return;
  }
  begin(*start, out);
}

void DeployedServer::begin(const net::NetStart& start, std::ostream& out) {
  server_.stopAt(options_.max_ticks);
  run_.players = start.header.player_count;
  out << "Starting " << run_.level << " for " << seatsText(server_.seated())
      << ", input delay " << static_cast<int>(start.input_delay)
      << (options_.input_delay ? " ticks\n" : " ticks (measured)\n");
  if (options_.mode == DeployedGameMode::SERVE) {
    world_ = DeployedNetWorld::create(options_.content, start.header, logic_);
    run_.logic = world_ && world_->world().hasLogic();
  }
  if (world_ && !options_.replay.empty()) {
    world_->startRecording(start.header);
  }
  progress_at_ = std::chrono::steady_clock::now();
  phase_ = DeployedServerPhase::RUNNING;
}

void DeployedServer::follow(std::ostream& out) {
  if (server_.desync()) {
    desynced_at_ = std::chrono::steady_clock::now();
    phase_ = DeployedServerPhase::COLLECTING;
  } else if (server_.playing() == 0) {
    out << "Every player has left\n";
    endRun(out);
  } else if (world_) {
    noticeStall(out);
    stepReference(out);
  } else if (server_.nextTick() >= options_.max_ticks) {
    endRun(out);  // Relaying: every frame of the run has gone out.
  } else {
    noticeStall(out);
  }
}

void DeployedServer::noticeStall(std::ostream& out) {
  const auto now = std::chrono::steady_clock::now();
  const uint64_t tick = server_.nextTick();
  if (tick != progress_tick_ || server_.waitingOn() == 0) {
    progress_tick_ = tick;
    progress_at_ = now;
    return;
  }
  const auto waited = now - progress_at_;
  if (waited >= options_.stall_drop) {
    dropStalled(out);
  } else if (waited >= DEPLOYED_STALL_NOTICE && announced_tick_ != tick) {
    announceStall(out);
  }
}

void DeployedServer::announceStall(std::ostream& out) {
  if (const auto said = server_.announceWaiting()) {
    announced_tick_ = said->tick;
    out << "Waiting for " << seatsText(said->waiting) << " at tick "
        << said->tick << '\n';
  }
}

void DeployedServer::dropStalled(std::ostream& out) {
  const uint8_t stalled = server_.waitingOn();
  for (uint8_t seat = 0; seat < sim::MAX_PLAYERS; ++seat) {
    if ((stalled & (1U << seat)) != 0) {
      server_.removeSeat(seat, net::NetRefusalReason::STALLED);
      out << "Dropped " << seatText(seat) << ": no input for "
          << options_.stall_drop.count() << " ms\n";
    }
  }
  progress_at_ = std::chrono::steady_clock::now();
}

void DeployedServer::stepReference(std::ostream& out) {
  while (server_.state() == net::NetServerState::RUNNING &&
         !world_->world().runOver() &&
         world_->nextTick() < options_.max_ticks) {
    const std::optional<net::NetFrame> frame = server_.takeFrame();
    if (!frame) {
      return;
    }
    keep(world_->step(*frame), out);
  }
  if (server_.state() == net::NetServerState::RUNNING) {
    endRun(out);
  }
}

void DeployedServer::keep(const sim::TickResult& result, std::ostream& out) {
  if (result.hash) {
    server_.reportHash(*result.hash);
    keepCheckpoint(*result.hash, checkpoints_);
    run_.hash = result.hash->combined;
  }
  for (const std::string& line : world_->takeLogicLog()) {
    out << "[logic " << result.tick << "] " << line << '\n';
  }
}

void DeployedServer::collectTraces(std::ostream& out) {
  const bool waited =
      std::chrono::steady_clock::now() - desynced_at_ >= DEPLOYED_TRACE_WAIT;
  if (server_.tracesComplete() || waited) {
    reportDesync();
    endRun(out);
  }
}

void DeployedServer::reportDesync() {
  const std::optional<net::NetDesync>& caught = server_.desync();
  if (!caught) {
    return;
  }
  const net::NetDesync& desync = *caught;
  const std::vector<net::NetPeerTrace> traces = server_.traces();
  const std::filesystem::path path =
      desyncReportPath(options_.desync_dir, run_.level, desync.tick);
  const bool written =
      writeText(path, desyncReport(run_.level, desync, traces));
  run_.error = describeDesync(desync, checkpoints_) + "; the run " +
               divergenceText(net::findTraceDivergence(traces)) +
               (written ? ". Report: " : ". Could not write the report to ") +
               path.string();
}

void DeployedServer::endRun(std::ostream& out) {
  server_.end();
  if (world_) {
    run_.ticks = world_->nextTick();
    run_.outcome = world_->world().outcome();
    if (const std::optional<sim::Replay> replay = world_->replay()) {
      saveReplay(options_, *replay, out);
    }
  } else {
    run_.ticks = server_.nextTick();
  }
  phase_ = DeployedServerPhase::FINISHED;
}

}  // namespace eng::editor
