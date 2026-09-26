#include "deployed-server.h"

#include "deployed-level.h"
#include "desync-text.h"

#include <bit>
#include <editor/deploy/deployed-content.h>
#include <utility>

namespace eng::editor {

namespace {

  /// The session a server for @p options runs.
  net::LockstepServerConfig sessionFor(const DeployedGameOptions& options) {
    net::LockstepServerConfig config;
    config.content_hash = deployedContentHash(options.content);
    config.input_delay = options.input_delay;
    config.frames = options.mode == DeployedGameMode::SERVE
                        ? net::NetServerFrames::KEEP
                        : net::NetServerFrames::RELAY;
    return config;
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
  out << "Starting " << run_.level << " for " << static_cast<int>(run_.players)
      << (run_.players == 1 ? " player\n" : " players\n");
  if (options_.mode == DeployedGameMode::SERVE) {
    world_ = DeployedNetWorld::create(options_.content, start, logic_);
    run_.logic = world_ && world_->world().hasLogic();
  }
  phase_ = DeployedServerPhase::RUNNING;
}

void DeployedServer::follow(std::ostream& out) {
  if (const auto& desync = server_.desync()) {
    run_.error = describeDesync(*desync, checkpoints_);
    endRun();
  } else if (server_.playing() == 0) {
    out << "Every player has left\n";
    endRun();
  } else if (world_) {
    stepReference(out);
  } else if (server_.nextTick() >= options_.max_ticks) {
    endRun();  // Relaying: every frame of the run has gone out.
  }
}

void DeployedServer::stepReference(std::ostream& out) {
  while (!world_->world().runOver() &&
         world_->nextTick() < options_.max_ticks) {
    const std::optional<net::NetFrame> frame = server_.takeFrame();
    if (!frame) {
      return;
    }
    keep(world_->step(*frame), out);
  }
  endRun();
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

void DeployedServer::endRun() {
  server_.end();
  if (world_) {
    run_.ticks = world_->nextTick();
    run_.outcome = world_->world().outcome();
  } else {
    run_.ticks = server_.nextTick();
  }
  phase_ = DeployedServerPhase::FINISHED;
}

}  // namespace eng::editor
