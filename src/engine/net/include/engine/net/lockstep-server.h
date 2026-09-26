#pragma once

/// @file lockstep-server.h
/// @brief The one server of a co-op session: seats, frames and hash checks.
/// @par Threading
/// Main-thread-only.

#include <array>
#include <cstdint>
#include <deque>
#include <engine/net/lockstep-server-config.h>
#include <engine/net/net-desync.h>
#include <engine/net/net-frame.h>
#include <engine/net/net-hash-check.h>
#include <engine/net/net-message.h>
#include <engine/net/net-peer-trace.h>
#include <engine/net/net-refusal.h>
#include <engine/net/net-server-seat.h>
#include <engine/net/net-transport.h>
#include <engine/sim/input-queue.h>
#include <engine/sim/tick-hash.h>
#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace eng::net {

/// Where a server's session stands.
enum class NetServerState : uint8_t {
  LOBBY,     ///< Seating players; no run
  RUNNING,   ///< Relaying a run's frames
  DESYNCED,  ///< A run diverged; frames stopped until `end`
};

/// The server every co-op session runs through (ADR-013), in a player's
/// game (a listen server) or on its own (dedicated). It seats clients,
/// starts runs at level boundaries, turns every seat's input into one
/// frame per tick once all of it is in — the lockstep stall condition of
/// ADR-005, `sim::InputQueue`'s — and compares the peers' hashes.
///
/// It knows nothing about the game: a dropped seat is marked absent in
/// the frames and each client fills it.
class LockstepServer {
public:
  /// A server in the lobby, reached through @p transport.
  LockstepServer(std::unique_ptr<NetTransport> transport,
                 const LockstepServerConfig& config);

  /// Handle everything the transport has: seat or refuse clients, take
  /// inputs and hash reports, and send every frame that is complete.
  void poll();

  /// Start a run of @p level with @p seed for every seated client, at
  /// tick 0; any run in progress is ended first. Its players are the
  /// seats up to the highest filled one; an empty seat below that plays
  /// absent. Nothing when nobody is seated; otherwise the start sent.
  std::optional<NetStart> start(const std::string& level, uint64_t seed);

  /// Tell the client in @p seat why, disconnect it, and free the seat at
  /// once; in a run, the seat plays absent from the next frame. What a
  /// server does with a seat that stopped sending input (`STALLED`) — the
  /// engine reads no clock, so whoever runs it decides how long is too long.
  void removeSeat(uint8_t seat, NetRefusalReason why);

  /// End the run, telling its clients, and go back to the lobby.
  void end();

  /// Send no frame for @p tick or later this run: the run's last tick is
  /// the one before. Clients then end on the same tick the server does.
  void stopAt(uint64_t tick) { stop_at_ = tick; }

  /// Compare the server's own hash for a tick, when it simulates the run
  /// too. Reported first, it is the one the clients' are checked against.
  void reportHash(const sim::TickHash& hash);

  /// Tell every client in the run who the next frame is waiting on, and
  /// give what was said; nothing — and nothing sent — when the run is not
  /// waiting on anyone. Whoever runs the server calls this once a wait has
  /// gone on long enough to be worth showing; the engine reads no clock.
  std::optional<NetWaiting> announceWaiting();

  /// The traces the peers of a desynced run have sent, and the server's
  /// own when it reported hashes, in seat order with its own last.
  [[nodiscard]] std::vector<NetPeerTrace> traces() const;

  /// Whether every seat playing the desynced run has sent its trace.
  [[nodiscard]] bool tracesComplete() const;

  /// The next frame sent and not yet taken, when frames are kept.
  std::optional<NetFrame> takeFrame();

  /// Where the session stands.
  [[nodiscard]] NetServerState state() const { return state_; }
  /// A bit per seat with a client in it.
  [[nodiscard]] uint8_t seated() const;
  /// A bit per seat playing the current run.
  [[nodiscard]] uint8_t playing() const;
  /// A bit per playing seat whose input for the next frame has not come:
  /// who the session is waiting on, for a stall to name.
  [[nodiscard]] uint8_t waitingOn() const;
  /// The tick the next frame will confirm.
  [[nodiscard]] uint64_t nextTick() const;
  /// The current run's start, while there is one.
  [[nodiscard]] const std::optional<NetStart>& run() const { return start_; }
  /// The divergence that halted the run, while `DESYNCED`.
  [[nodiscard]] const std::optional<NetDesync>& desync() const {
    return desync_;
  }

private:
  /// Act on one event from the transport.
  void handle(const NetEvent& event);
  /// Act on @p message from @p peer.
  void receive(NetPeer peer, const NetMessage& message);
  /// Seat the client on @p peer, or refuse it.
  void on(NetPeer peer, const NetHello& hello);
  /// Take an input from the client on @p peer.
  void on(NetPeer peer, const NetInput& input);
  /// Take a hash report from the client on @p peer.
  void on(NetPeer peer, const NetHashReport& report);
  /// Keep the trace the client on @p peer sent after a desync.
  void on(NetPeer peer, const NetTrace& trace);
  /// Nothing: no other message is a client's to send.
  template <typename Message> void on(NetPeer /*peer*/, const Message& /*m*/) {}
  /// Why @p hello cannot be seated, or nothing when it can.
  [[nodiscard]] std::optional<NetRefusalReason>
  refusalFor(const NetHello& hello) const;
  /// Compare @p report from @p slot with the first of its tick.
  void check(uint8_t slot, const NetHashReport& report);
  /// Stop the run for @p desync, telling every client.
  void halt(const NetDesync& desync);
  /// The start of the next run, of @p level with @p seed.
  [[nodiscard]] NetStart startOf(const std::string& level, uint64_t seed);
  /// Set up the queue and seats for the run @p start begins.
  void seatRun(const NetStart& start);
  /// The input delay of the next run, as configured or measured.
  [[nodiscard]] uint8_t delayForRun() const;
  /// Queue no input in @p queue for each of @p players' seats for the
  /// ticks before the input delay.
  void prefillDelay(sim::InputQueue& queue, const NetStart& start) const;
  /// Free the seat of @p peer, which has gone.
  void drop(NetPeer peer);
  /// Send every frame whose inputs are all in.
  void relay();
  /// Send @p frame to the run's clients, keeping it when frames are kept.
  void sendFrame(const NetFrame& frame);
  /// Fill the absent seats' input for @p queue's next tick.
  void fillAbsent(sim::InputQueue& queue) const;
  /// Send @p message to every client in the run.
  void broadcast(const NetMessage& message);
  /// Send @p message to every seated client.
  void broadcastSeated(const NetMessage& message);
  /// Send @p message to @p peer.
  void send(NetPeer peer, const NetMessage& message);
  /// The seat of @p peer, if it has one.
  [[nodiscard]] std::optional<uint8_t> seatOf(NetPeer peer) const;
  /// The lowest free seat, if there is one.
  [[nodiscard]] std::optional<uint8_t> freeSeat() const;
  /// Seats in the current run with no player.
  [[nodiscard]] uint8_t absent() const;

  /// The connection to every client.
  std::unique_ptr<NetTransport> transport_;
  /// What the server was made with.
  LockstepServerConfig config_;
  /// Where the session stands.
  NetServerState state_ = NetServerState::LOBBY;
  /// The seats, by input slot.
  std::array<NetServerSeat, sim::MAX_PLAYERS> seats_{};
  /// The current run's start.
  std::optional<NetStart> start_;
  /// The current run's inputs, until each tick's are all in.
  std::optional<sim::InputQueue> queue_;
  /// The first tick no frame is sent for.
  uint64_t stop_at_ = UINT64_MAX;
  /// The number the next run gets.
  uint16_t next_run_ = 0;
  /// Hash reports by `(tick / NET_HASH_INTERVAL) % NET_HASH_RING`.
  std::array<NetHashCheck, NET_HASH_RING> checks_{};
  /// The divergence that halted the run.
  std::optional<NetDesync> desync_;
  /// Frames sent and not yet taken, when kept.
  std::deque<NetFrame> frames_;
  /// Each seat's trace of the desynced run, once sent.
  std::array<std::optional<NetTrace>, sim::MAX_PLAYERS> traces_{};
  /// The server's own recent hashes, when it reports them.
  std::deque<sim::TickHash> own_trace_;
};

}  // namespace eng::net
