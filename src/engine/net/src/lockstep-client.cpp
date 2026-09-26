#include "trace-ring.h"

#include <engine/net/lockstep-client.h>
#include <engine/net/net-codec.h>
#include <engine/net/net-hash-report.h>
#include <utility>

namespace eng::net {

LockstepClient::LockstepClient(std::unique_ptr<NetTransport> transport,
                               NetHello hello)
  : transport_(std::move(transport)), hello_(std::move(hello)) {}

void LockstepClient::poll() {
  while (const std::optional<NetEvent> event = transport_->poll()) {
    handle(*event);
  }
}

void LockstepClient::handle(const NetEvent& event) {
  if (event.kind == NetEventKind::CONNECTED) {
    server_ = event.peer;
    send(hello_);
    return;
  }
  if (event.kind == NetEventKind::DISCONNECTED) {
    // Refused stays refused: the disconnect is how a refusal ends.
    state_ = state_ == NetClientState::REFUSED ? state_
                                               : NetClientState::DISCONNECTED;
    server_.reset();
    return;
  }
  if (const std::optional<NetMessage> message = decodeNetMessage(event.bytes)) {
    std::visit([this](const auto& body) { on(body); }, *message);
  }
}

void LockstepClient::on(const NetWelcome& welcome) {
  slot_ = welcome.slot;
  state_ = NetClientState::SEATED;
}

void LockstepClient::on(const NetRefusal& refusal) {
  refusal_ = refusal.reason;
  state_ = NetClientState::REFUSED;
}

void LockstepClient::on(const NetRoster& roster) {
  roster_ = roster.seated;
}

void LockstepClient::on(const NetStart& start) {
  run_ = start;
  pending_start_ = start;
  frames_.clear();
  desync_.reset();
  waiting_.reset();
  trace_.clear();
  next_frame_ = 0;
  next_input_ = start.input_delay;
  state_ = NetClientState::PLAYING;
}

void LockstepClient::on(const NetFrame& frame) {
  if (current(frame.run) && state_ == NetClientState::PLAYING) {
    frames_.push_back(frame);
    if (waiting_ && frame.tick >= waiting_->tick) {
      waiting_.reset();
    }
  }
}

void LockstepClient::on(const NetDesync& desync) {
  if (current(desync.run)) {
    desync_ = desync;
    frames_.clear();
    waiting_.reset();
    state_ = NetClientState::DESYNCED;
    send(traceOf(desync.run, trace_));
  }
}

void LockstepClient::on(const NetWaiting& waiting) {
  if (current(waiting.run) && state_ == NetClientState::PLAYING &&
      waiting.tick >= next_frame_ + frames_.size()) {
    waiting_ = waiting;
  }
}

void LockstepClient::on(const NetEnd& end) {
  if (current(end.run) && state_ == NetClientState::PLAYING) {
    state_ = NetClientState::SEATED;
  }
}

bool LockstepClient::current(uint16_t run) const {
  return run_ && run_->run == run;
}

bool LockstepClient::sendInput(const sim::PlayerInput& input) {
  if (state_ != NetClientState::PLAYING || !run_ ||
      next_input_ >= next_frame_ + run_->input_delay) {
    return false;
  }
  send(NetInput{run_->run, next_input_, input});
  ++next_input_;
  return true;
}

std::optional<NetFrame> LockstepClient::takeFrame() {
  if (frames_.empty()) {
    return std::nullopt;
  }
  NetFrame frame = frames_.front();
  frames_.pop_front();
  next_frame_ = frame.tick + 1;
  return frame;
}

void LockstepClient::reportHash(const sim::TickHash& hash) {
  if (state_ != NetClientState::PLAYING || !run_) {
    return;
  }
  keepTraced(hash, trace_);
  if (hash.tick % NET_HASH_INTERVAL == 0) {
    send(NetHashReport{run_->run, hash});
  }
}

std::optional<NetStart> LockstepClient::takeStart() {
  return std::exchange(pending_start_, std::nullopt);
}

void LockstepClient::send(const NetMessage& message) {
  if (server_) {
    transport_->send(*server_, encodeNetMessage(message));
  }
}

}  // namespace eng::net
