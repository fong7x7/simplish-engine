#include <algorithm>
#include <bit>
#include <engine/core/assert.h>
#include <engine/net/lockstep-server.h>
#include <engine/net/net-codec.h>
#include <utility>

namespace eng::net {

namespace {

  /// The bit for @p seat.
  uint8_t bit(uint8_t seat) {
    return static_cast<uint8_t>(1U << seat);
  }

  /// The first section @p expected and @p actual differ on, or nothing when
  /// they agree.
  std::optional<uint8_t> differingSection(const sim::TickHash& expected,
                                          const sim::TickHash& actual) {
    if (expected.combined == actual.combined) {
      return std::nullopt;
    }
    if (expected.section_count != actual.section_count) {
      return NET_SECTION_COUNT_DIFFERS;
    }
    for (std::size_t i = 0; i < expected.section_count; ++i) {
      if (expected.sections[i].hash != actual.sections[i].hash) {
        return static_cast<uint8_t>(i);
      }
    }
    return NET_SECTION_COUNT_DIFFERS;
  }

}  // namespace

LockstepServer::LockstepServer(std::unique_ptr<NetTransport> transport,
                               const LockstepServerConfig& config)
  : transport_(std::move(transport)), config_(config) {
  // NOLINTNEXTLINE(cppcoreguidelines-avoid-do-while,readability-simplify-boolean-expr)
  ENGINE_ASSERT(config.seats >= 1 && config.seats <= sim::MAX_PLAYERS,
                "a server has between 1 and MAX_PLAYERS seats");
  // NOLINTNEXTLINE(cppcoreguidelines-avoid-do-while,readability-simplify-boolean-expr)
  ENGINE_ASSERT(config.input_delay >= 1, "input delay is at least a tick");
}

void LockstepServer::poll() {
  while (const std::optional<NetEvent> event = transport_->poll()) {
    handle(*event);
  }
  relay();
}

void LockstepServer::handle(const NetEvent& event) {
  if (event.kind == NetEventKind::DISCONNECTED) {
    drop(event.peer);
    return;
  }
  if (event.kind != NetEventKind::RECEIVED) {
    return;  // A connection is nothing until it says hello.
  }
  if (const std::optional<NetMessage> message = decodeNetMessage(event.bytes)) {
    receive(event.peer, *message);
  }
}

void LockstepServer::receive(NetPeer peer, const NetMessage& message) {
  std::visit([this, peer](const auto& body) { on(peer, body); }, message);
}

std::optional<NetRefusalReason>
LockstepServer::refusalFor(const NetHello& hello) const {
  if (hello.protocol != NET_PROTOCOL_VERSION) {
    return NetRefusalReason::PROTOCOL;
  }
  if (hello.content_hash != config_.content_hash) {
    return NetRefusalReason::CONTENT;
  }
  return freeSeat() ? std::nullopt : std::optional{NetRefusalReason::FULL};
}

void LockstepServer::on(NetPeer peer, const NetHello& hello) {
  if (seatOf(peer)) {
    return;
  }
  const std::optional<uint8_t> seat = freeSeat();
  if (const auto reason = refusalFor(hello); reason || !seat) {
    send(peer, NetRefusal{reason.value_or(NetRefusalReason::FULL)});
    transport_->disconnect(peer);
    return;
  }
  seats_[*seat] = {peer, 1, 0, 0, hello.character};
  send(peer, NetWelcome{*seat});
  broadcastSeated(NetRoster{seated()});
}

void LockstepServer::on(NetPeer peer, const NetInput& input) {
  const std::optional<uint8_t> seat = seatOf(peer);
  if (!seat || state_ != NetServerState::RUNNING || !start_ || !queue_ ||
      input.run != start_->run || seats_[*seat].playing == 0) {
    return;
  }
  if (queue_->submit(*seat, input.tick, input.input) ==
      sim::InputSubmitResult::ACCEPTED) {
    NetServerSeat& taken = seats_[*seat];
    taken.next_input = std::max(taken.next_input, input.tick + 1);
  }
}

void LockstepServer::on(NetPeer peer, const NetHashReport& report) {
  const std::optional<uint8_t> seat = seatOf(peer);
  if (seat && seats_[*seat].playing != 0) {
    check(*seat, report);
  }
}

void LockstepServer::reportHash(const sim::TickHash& hash) {
  if (start_) {
    check(NET_SERVER_SLOT, {start_->run, hash});
  }
}

void LockstepServer::check(uint8_t slot, const NetHashReport& report) {
  const uint64_t tick = report.hash.tick;
  if (state_ != NetServerState::RUNNING || !start_ ||
      report.run != start_->run || tick % NET_HASH_INTERVAL != 0) {
    return;
  }
  NetHashCheck& first = checks_[(tick / NET_HASH_INTERVAL) % NET_HASH_RING];
  if (first.taken == 0 || first.run != report.run || first.hash.tick < tick) {
    first = {report.run, report.hash, 1};
    return;
  }
  if (first.hash.tick != tick) {
    return;  // So far behind its slot has been reused: nothing to compare.
  }
  if (const auto section = differingSection(first.hash, report.hash)) {
    halt({report.run, tick, *section, slot});
  }
}

void LockstepServer::halt(const NetDesync& desync) {
  desync_ = desync;
  state_ = NetServerState::DESYNCED;
  broadcast(desync);
}

void LockstepServer::drop(NetPeer peer) {
  const std::optional<uint8_t> seat = seatOf(peer);
  if (!seat) {
    return;
  }
  seats_[*seat] = NetServerSeat{};
  broadcastSeated(NetRoster{seated()});
}

std::optional<NetStart> LockstepServer::start(const std::string& level,
                                              uint64_t seed) {
  if (seated() == 0) {
    return std::nullopt;
  }
  end();
  NetStart start = startOf(level, seed);
  seatRun(start.header.player_count);
  start_ = start;
  state_ = NetServerState::RUNNING;
  broadcast(start);
  relay();
  return start;
}

NetStart LockstepServer::startOf(const std::string& level, uint64_t seed) {
  // Every seat up to the highest filled one plays; an empty one is absent.
  const auto players = static_cast<uint8_t>(8 - std::countl_zero(seated()));
  NetStart start{next_run_++,
                 {level, config_.content_hash, seed, players, {}},
                 config_.input_delay};
  for (uint8_t seat = 0; seat < players; ++seat) {
    start.header.characters[seat] = seats_[seat].character;
  }
  return start;
}

void LockstepServer::seatRun(uint8_t players) {
  prefillDelay(queue_.emplace(players), players);
  for (uint8_t seat = 0; seat < sim::MAX_PLAYERS; ++seat) {
    NetServerSeat& taken = seats_[seat];
    taken.playing = seat < players ? taken.connected : 0;
    taken.next_input = config_.input_delay;
  }
  checks_ = {};
  desync_.reset();
  frames_.clear();
  stop_at_ = UINT64_MAX;
}

void LockstepServer::prefillDelay(sim::InputQueue& queue,
                                  uint8_t players) const {
  // Nobody can have sent input for the first `input_delay` ticks: they run
  // on none, the same for every peer.
  for (uint64_t tick = 0; tick < config_.input_delay; ++tick) {
    for (uint8_t seat = 0; seat < players; ++seat) {
      (void)queue.submit(seat, tick, {});
    }
  }
}

void LockstepServer::end() {
  if (start_) {
    broadcast(NetEnd{start_->run});
  }
  for (NetServerSeat& seat : seats_) {
    seat.playing = 0;
  }
  start_.reset();
  queue_.reset();
  state_ = NetServerState::LOBBY;
}

void LockstepServer::relay() {
  if (state_ != NetServerState::RUNNING || playing() == 0 || !queue_ ||
      !start_) {
    return;
  }
  sim::InputQueue& queue = *queue_;
  const uint16_t run = start_->run;
  while (queue.nextTick() < stop_at_) {
    fillAbsent(queue);
    const uint64_t tick = queue.nextTick();
    const std::optional<sim::TickInput> input = queue.take();
    if (!input) {
      return;
    }
    sendFrame({run, tick, absent(), *input});
  }
}

void LockstepServer::sendFrame(const NetFrame& frame) {
  broadcast(frame);
  if (config_.frames == NetServerFrames::KEEP) {
    frames_.push_back(frame);
  }
}

void LockstepServer::fillAbsent(sim::InputQueue& queue) const {
  const uint8_t missing = absent();
  for (uint8_t seat = 0; seat < sim::MAX_PLAYERS; ++seat) {
    if ((missing & bit(seat)) != 0) {
      // A seat that sent input before it dropped keeps it: DUPLICATE.
      (void)queue.submit(seat, queue.nextTick(), {});
    }
  }
}

std::optional<NetFrame> LockstepServer::takeFrame() {
  if (frames_.empty()) {
    return std::nullopt;
  }
  NetFrame frame = frames_.front();
  frames_.pop_front();
  return frame;
}

void LockstepServer::broadcast(const NetMessage& message) {
  const std::vector<std::byte> bytes = encodeNetMessage(message);
  for (const NetServerSeat& seat : seats_) {
    if (seat.playing != 0) {
      transport_->send(seat.peer, bytes);
    }
  }
}

void LockstepServer::broadcastSeated(const NetMessage& message) {
  const std::vector<std::byte> bytes = encodeNetMessage(message);
  for (const NetServerSeat& seat : seats_) {
    if (seat.connected != 0) {
      transport_->send(seat.peer, bytes);
    }
  }
}

void LockstepServer::send(NetPeer peer, const NetMessage& message) {
  transport_->send(peer, encodeNetMessage(message));
}

std::optional<uint8_t> LockstepServer::seatOf(NetPeer peer) const {
  for (uint8_t seat = 0; seat < sim::MAX_PLAYERS; ++seat) {
    if (seats_[seat].connected != 0 && seats_[seat].peer == peer) {
      return seat;
    }
  }
  return std::nullopt;
}

std::optional<uint8_t> LockstepServer::freeSeat() const {
  for (uint8_t seat = 0; seat < config_.seats; ++seat) {
    if (seats_[seat].connected == 0) {
      return seat;
    }
  }
  return std::nullopt;
}

uint8_t LockstepServer::seated() const {
  uint8_t mask = 0;
  for (uint8_t seat = 0; seat < sim::MAX_PLAYERS; ++seat) {
    mask |= seats_[seat].connected != 0 ? bit(seat) : 0;
  }
  return mask;
}

uint8_t LockstepServer::playing() const {
  uint8_t mask = 0;
  for (uint8_t seat = 0; seat < sim::MAX_PLAYERS; ++seat) {
    mask |= seats_[seat].playing != 0 ? bit(seat) : 0;
  }
  return mask;
}

uint8_t LockstepServer::absent() const {
  if (!start_) {
    return 0;
  }
  const auto in_run =
      static_cast<uint8_t>(bit(start_->header.player_count) - 1);
  return static_cast<uint8_t>(in_run & ~playing());
}

uint8_t LockstepServer::waitingOn() const {
  if (state_ != NetServerState::RUNNING || !queue_) {
    return 0;
  }
  const uint64_t next = queue_->nextTick();
  uint8_t mask = 0;
  for (uint8_t seat = 0; seat < sim::MAX_PLAYERS; ++seat) {
    const NetServerSeat& taken = seats_[seat];
    const bool behind = taken.next_input <= next;
    mask |= taken.playing != 0 && behind ? bit(seat) : 0;
  }
  return mask;
}

uint64_t LockstepServer::nextTick() const {
  return queue_ ? queue_->nextTick() : 0;
}

}  // namespace eng::net
