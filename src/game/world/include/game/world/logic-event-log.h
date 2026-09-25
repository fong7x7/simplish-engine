#pragma once

/// @file logic-event-log.h
/// @brief What happened in a tick, kept for the game logic to hear next.
/// @par Threading
/// Main-thread-only; filled inside a tick.

#include <engine/sim/state-hasher.h>
#include <game/logic/logic-event.h>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace eng::game {

/// The events a `GameWorld` with game logic tells it of: noted during one
/// tick where each happens — a hurt, a death or a downing as each hit is
/// applied, a spawn or a removal as the logic's writes are — and handed to
/// the logic on the next, when the dead are gone and could not otherwise be
/// asked where they fell.
///
/// Carried from one tick to the next, so it is state, and hashed.
class LogicEventLog {
public:
  /// Note @p event — copying its name and state, which need live only for
  /// the call.
  void note(const LogicEvent& event);
  /// Make the tick's events the ones `events` gives, and start gathering
  /// the next tick's. Last thing a tick does with the log.
  void publish();
  /// The last tick's events, in the order noticed.
  [[nodiscard]] std::span<const LogicEvent> events() const { return events_; }
  /// Fold the last tick's events into @p hasher.
  void hashInto(sim::StateHasher& hasher) const;

private:
  /// The last tick's events, their ids viewing `ids_`.
  std::vector<LogicEvent> events_;
  /// The names `events_` view.
  std::vector<std::string> ids_;
  /// The state ids `events_` view.
  std::vector<std::string> states_;
  /// This tick's events so far; their ids are in `pending_ids_`.
  std::vector<LogicEvent> pending_;
  /// Each pending event's name.
  std::vector<std::string> pending_ids_;
  /// Each pending event's state id.
  std::vector<std::string> pending_states_;
};

}  // namespace eng::game
