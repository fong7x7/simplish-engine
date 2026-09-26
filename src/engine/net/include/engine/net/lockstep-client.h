#pragma once

/// @file lockstep-client.h
/// @brief One player's end of a co-op session.
/// @par Threading
/// Main-thread-only.

#include <cstdint>
#include <deque>
#include <engine/net/net-desync.h>
#include <engine/net/net-frame.h>
#include <engine/net/net-hello.h>
#include <engine/net/net-message.h>
#include <engine/net/net-refusal.h>
#include <engine/net/net-start.h>
#include <engine/net/net-transport.h>
#include <engine/net/net-waiting.h>
#include <engine/sim/player-input.h>
#include <engine/sim/tick-hash.h>
#include <memory>
#include <optional>

namespace eng::net {

/// Where a client stands.
enum class NetClientState : uint8_t {
  CONNECTING,    ///< Waiting for the connection, then for a seat
  SEATED,        ///< In a seat, between runs
  PLAYING,       ///< In a run
  DESYNCED,      ///< The run diverged and is halted
  REFUSED,       ///< The server would not seat it
  DISCONNECTED,  ///< The connection is gone
};

/// A client of a `LockstepServer` (ADR-013). The caller samples its
/// player's input on its own clock and sends it; the simulation steps on
/// the frames that come back, never on unconfirmed input:
///
///     client.poll();
///     while (auto frame = client.takeFrame()) { /* fill absent, step */ }
///     client.sendInput(sampled);   // for tick nextInputTick()
///
/// Input runs `input_delay` ticks ahead of the frames taken and no more:
/// while the session waits on someone, `sendInput` refuses, and that
/// refusal is the local stall.
class LockstepClient {
public:
  /// A client connecting through @p transport, and asking for a seat with
  /// @p hello once connected.
  LockstepClient(std::unique_ptr<NetTransport> transport, NetHello hello);

  /// Handle everything the transport has.
  void poll();

  /// Send @p input for `nextInputTick()`, and move on to the next tick.
  /// False — nothing sent — when not in a run, or when input is already
  /// `input_delay` ticks ahead of the frames taken.
  bool sendInput(const sim::PlayerInput& input);

  /// The next frame of the run, in tick order, once it has arrived.
  std::optional<NetFrame> takeFrame();

  /// Keep @p hash — every tick's, in order — for a trace should the run
  /// desync, and send it to the server when its tick is one compared.
  /// Told of a desync, the client sends its trace by itself.
  void reportHash(const sim::TickHash& hash);

  /// The start of a run that has begun since the last call: build the
  /// simulation from it. Nothing when none has.
  std::optional<NetStart> takeStart();

  /// Where the client stands.
  [[nodiscard]] NetClientState state() const { return state_; }
  /// The seat, once given one.
  [[nodiscard]] std::optional<uint8_t> slot() const { return slot_; }
  /// A bit per seat with a client in it, as last told.
  [[nodiscard]] uint8_t roster() const { return roster_; }
  /// The run being played, or last played.
  [[nodiscard]] const std::optional<NetStart>& run() const { return run_; }
  /// Why the server refused, when it did.
  [[nodiscard]] std::optional<NetRefusalReason> refusal() const {
    return refusal_;
  }
  /// The divergence that halted the run, when one did.
  [[nodiscard]] const std::optional<NetDesync>& desync() const {
    return desync_;
  }
  /// Who the session was last said to be waiting on, until the frame it
  /// was waiting for arrives: what to show while stalled.
  [[nodiscard]] const std::optional<NetWaiting>& waiting() const {
    return waiting_;
  }
  /// The tick the next `sendInput` is for.
  [[nodiscard]] uint64_t nextInputTick() const { return next_input_; }
  /// The tick the next `takeFrame` gives.
  [[nodiscard]] uint64_t nextFrameTick() const { return next_frame_; }
  /// Frames arrived and not yet taken.
  [[nodiscard]] std::size_t framesWaiting() const { return frames_.size(); }

private:
  /// Act on one event from the transport.
  void handle(const NetEvent& event);
  /// Take the seat the server gave.
  void on(const NetWelcome& welcome);
  /// Take the server's refusal.
  void on(const NetRefusal& refusal);
  /// Take who is seated.
  void on(const NetRoster& roster);
  /// Begin the run a start describes.
  void on(const NetStart& start);
  /// Keep a frame, when it belongs to the run.
  void on(const NetFrame& frame);
  /// Halt the run, when the desync is about it.
  void on(const NetDesync& desync);
  /// Leave the run, when the end is about it.
  void on(const NetEnd& end);
  /// Take who the session is waiting on, when it is about the run.
  void on(const NetWaiting& waiting);
  /// Nothing: no other message is a server's to send.
  template <typename Message> void on(const Message& /*message*/) {}
  /// Whether @p run is the one being played.
  [[nodiscard]] bool current(uint16_t run) const;
  /// Send @p message to the server.
  void send(const NetMessage& message);

  /// The connection to the server.
  std::unique_ptr<NetTransport> transport_;
  /// What the client asks for a seat with.
  NetHello hello_;
  /// The server's peer number, once connected.
  std::optional<NetPeer> server_;
  /// Where the client stands.
  NetClientState state_ = NetClientState::CONNECTING;
  /// The seat.
  std::optional<uint8_t> slot_;
  /// Seats with a client in them.
  uint8_t roster_ = 0;
  /// The run being played, or last played.
  std::optional<NetStart> run_;
  /// A start not yet taken.
  std::optional<NetStart> pending_start_;
  /// Why the server refused.
  std::optional<NetRefusalReason> refusal_;
  /// The divergence that halted the run.
  std::optional<NetDesync> desync_;
  /// Who the session is waiting on, while it is.
  std::optional<NetWaiting> waiting_;
  /// This run's recent hashes, for a trace.
  std::deque<sim::TickHash> trace_;
  /// Frames arrived and not yet taken, in tick order.
  std::deque<NetFrame> frames_;
  /// The tick the next input sent is for.
  uint64_t next_input_ = 0;
  /// The tick the next frame taken confirms.
  uint64_t next_frame_ = 0;
};

}  // namespace eng::net
