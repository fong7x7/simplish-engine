#include "deployed-client.h"

#include "deployed-replay.h"
#include "desync-text.h"
#include "seats-text.h"

#include <editor/deploy/deployed-content.h>
#include <game/world/stand-in-input.h>
#include <string_view>
#include <utility>

namespace eng::editor {

namespace {

  /// What a client of the content at @p content says when it asks to sit.
  net::NetHello helloFor(const std::filesystem::path& content) {
    return {net::NET_PROTOCOL_VERSION, deployedContentHash(content), ""};
  }

  /// Why the server refused, in words.
  std::string_view refusalText(net::NetRefusalReason reason) {
    if (reason == net::NetRefusalReason::PROTOCOL) {
      return "The server runs another version of the protocol";
    }
    if (reason == net::NetRefusalReason::CONTENT) {
      return "The server's game content is not this game's";
    }
    return "The server is full";
  }

}  // namespace

DeployedClient::DeployedClient(std::unique_ptr<net::NetTransport> transport,
                               const DeployedGameOptions& options,
                               game::GameLogicFactory logic)
  : options_(options), logic_(logic),
    client_(std::move(transport), helloFor(options.content)) {}

void DeployedClient::poll(uint32_t due, std::ostream& out) {
  if (finished()) {
    return;
  }
  client_.poll();
  if (const std::optional<net::NetStart> start = client_.takeStart()) {
    begin(*start, out);
  }
  if (world_) {
    stepFrames(out);
    sendInputs(due);
    noticeWaiting(out);
  }
  checkFinished(out);
}

void DeployedClient::begin(const net::NetStart& start, std::ostream& out) {
  run_ = {};
  run_.level = start.header.level_id;
  run_.players = start.header.player_count;
  checkpoints_.clear();
  world_ = DeployedNetWorld::create(options_.content, start.header, logic_);
  if (!world_) {
    run_.error = "No level " + run_.level + " in this game";
    finished_ = 1;
    return;
  }
  run_.logic = world_->world().hasLogic();
  if (!options_.replay.empty()) {
    world_->startRecording(start.header);
  }
  out << "Playing " << run_.level << " as player "
      << static_cast<int>(client_.slot().value_or(0)) + 1 << '\n';
}

bool DeployedClient::worldOver() const {
  return world_ && world_->world().runOver();
}

void DeployedClient::stepFrames(std::ostream& out) {
  while (!worldOver()) {
    const std::optional<net::NetFrame> frame = client_.takeFrame();
    if (!frame) {
      return;
    }
    keep(world_->step(*frame), out);
  }
}

void DeployedClient::keep(const sim::TickResult& result, std::ostream& out) {
  if (result.hash) {
    client_.reportHash(*result.hash);
    keepCheckpoint(*result.hash, checkpoints_);
    run_.hash = result.hash->combined;
  }
  for (const std::string& line : world_->takeLogicLog()) {
    out << "[logic " << result.tick << "] " << line << '\n';
  }
}

void DeployedClient::sendInputs(uint32_t due) {
  const uint8_t slot = client_.slot().value_or(0);
  for (uint32_t i = 0; i < due && !worldOver(); ++i) {
    if (!client_.sendInput(game::standInInput(world_->world(), slot))) {
      return;  // The session is waiting on someone: the stall.
    }
  }
}

std::string DeployedClient::failure() const {
  using net::NetClientState;
  switch (client_.state()) {
    case NetClientState::REFUSED:
      return std::string(
          refusalText(client_.refusal().value_or(net::NetRefusalReason::FULL)));
    case NetClientState::DESYNCED:
      return client_.desync() ? describeDesync(*client_.desync(), checkpoints_)
                              : std::string("Desync");
    case NetClientState::DISCONNECTED:
      return world_ ? "Lost the server at tick " +
                          std::to_string(world_->nextTick())
                    : std::string("Could not reach the server");
    default:
      return {};
  }
}

void DeployedClient::noticeWaiting(std::ostream& out) {
  const std::optional<net::NetWaiting>& waiting = client_.waiting();
  if (!waiting || told_waiting_ == waiting->tick) {
    return;
  }
  told_waiting_ = waiting->tick;
  // A client that was itself the hold-up has nobody to blame but itself.
  const auto others = static_cast<uint8_t>(waiting->waiting &
                                           ~(1U << client_.slot().value_or(0)));
  if (others != 0) {
    out << "Waiting for " << seatsText(others) << '\n';
  }
}

void DeployedClient::checkFinished(std::ostream& out) {
  const net::NetClientState state = client_.state();
  const bool ended = state == net::NetClientState::SEATED && world_ &&
                     (client_.framesWaiting() == 0 || worldOver());
  const bool failed = state == net::NetClientState::REFUSED ||
                      state == net::NetClientState::DESYNCED ||
                      state == net::NetClientState::DISCONNECTED;
  if (failed) {
    run_.error = failure();
  }
  if (ended || failed) {
    finish(out);
  }
}

void DeployedClient::finish(std::ostream& out) {
  if (world_) {
    run_.ticks = world_->nextTick();
    run_.outcome = world_->world().outcome();
    if (const std::optional<sim::Replay> replay = world_->replay()) {
      saveReplay(options_, *replay, out);
    }
  }
  finished_ = 1;
}

}  // namespace eng::editor
