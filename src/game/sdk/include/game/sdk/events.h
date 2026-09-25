#pragma once

/// @file events.h
/// @brief The logic's own events: raised by one part, heard by others.
/// @par Threading
/// A value type; a member of the logic's.

#include <cstddef>
#include <functional>
#include <game/logic/game-logic-hash.h>
#include <game/logic/game-logic-world.h>
#include <utility>
#include <vector>

namespace eng::game::sdk {

/// A queue of the logic's own events of one type, and whoever listens for
/// them: so one part of a logic can say "wave 3 started" or "the boss is
/// enraged" and every other part that cares hears it, without the parts
/// calling each other.
///
/// `emit` queues; `dispatch` hands each queued event, in the order emitted,
/// to every handler, in the order subscribed — and an event emitted by a
/// handler is heard in the same dispatch, after those before it. Nothing
/// is heard until the logic dispatches, so where in its tick it does is
/// where its events are heard. Handlers are subscribed once, in the
/// logic's constructor or `onStart`, never while dispatching.
///
/// @code
///   struct WaveStarted { uint32_t wave; };
///   sdk::Events<WaveStarted> waves_;
///
///   void onStart(GameLogicWorld&) override {
///     waves_.subscribe([this](GameLogicWorld& world, const WaveStarted& e) {
///       spawnWave(world, e.wave);
///     });
///   }
///   void onTick(GameLogicWorld& world) override {
///     if (WAVES.due(world.tick())) waves_.emit({++wave_});
///     waves_.dispatch(world);
///   }
///   void onHash(GameLogicHash& hash) const override { waves_.hashInto(hash); }
/// @endcode
template <typename Event> class Events {
public:
  /// What hears an event: called with the world and the event.
  using Handler = std::function<void(GameLogicWorld&, const Event&)>;

  /// Have @p handler hear every event dispatched from now on, after the
  /// handlers subscribed before it.
  void subscribe(Handler handler) { handlers_.push_back(std::move(handler)); }

  /// Queue @p event, to be heard at the next `dispatch`.
  void emit(const Event& event) { queue_.push_back(event); }

  /// Hand every queued event to every handler, in order, including those
  /// the handlers emit; the queue is empty after.
  void dispatch(GameLogicWorld& world);

  /// Events queued and not yet heard.
  [[nodiscard]] size_t pending() const { return queue_.size(); }

  /// Fold the events not yet heard into @p hash, in order; each must be
  /// something `GameLogicHash::add` takes. Handlers are not state.
  void hashInto(GameLogicHash& hash) const;

private:
  /// Whoever listens, in the order they subscribed.
  std::vector<Handler> handlers_{};
  /// Events emitted and not yet heard, in the order emitted.
  std::vector<Event> queue_{};
};

template <typename Event> void Events<Event>::dispatch(GameLogicWorld& world) {
  // By index, and by copy: a handler may emit, growing the queue.
  for (size_t i = 0; i < queue_.size(); ++i) {
    const Event event = queue_[i];
    for (const Handler& handler : handlers_) {
      handler(world, event);
    }
  }
  queue_.clear();
}

template <typename Event>
void Events<Event>::hashInto(GameLogicHash& hash) const {
  hash.add(queue_.size());
  for (const Event& event : queue_) {
    hash.add(event);
  }
}

}  // namespace eng::game::sdk
