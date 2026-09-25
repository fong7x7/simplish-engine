#pragma once

/// @file logic-event-log.h
/// @brief What happened in a tick, kept for the game logic to hear next.
/// @par Threading
/// Main-thread-only; filled inside a tick.

#include <cstdint>
#include <engine/math/vec3.h>
#include <engine/sim/entity-handle.h>
#include <engine/sim/state-hasher.h>
#include <game/actors/actor-pool.h>
#include <game/logic/logic-event-kind.h>
#include <game/logic/logic-event.h>
#include <game/logic/logic-target.h>
#include <game/player/player-pool.h>
#include <game/world/logic-slot-notes.h>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace eng::game {

/// The events a `GameWorld` with game logic tells it of: gathered during
/// one tick — spawns and removals as the logic's writes are applied, hurts,
/// deaths and downs read off the pools at its end — and handed to the logic
/// on the next, when the dead are gone and could not otherwise be asked
/// where they fell.
///
/// Carried from one tick to the next, so it is state, and hashed.
class LogicEventLog {
public:
  /// Note @p event — copying its name, which need live only for the call.
  void note(const LogicEvent& event);
  /// Note that the actor @p handle was removed rather than killed, so the
  /// end of the tick does not report it dead.
  void noteRemoved(sim::EntityHandle handle);
  /// At the end of @p tick, before compaction: note every actor hurt or
  /// killed on it, named and credited as @p notes say. The removed are not
  /// reported dead.
  void noteActors(const ActorPool& actors, const LogicSlotNotes& notes,
                  uint64_t tick);
  /// At the end of @p tick: note every player hurt or downed on it,
  /// credited as @p notes say.
  void notePlayers(const PlayerPool& players, const LogicSlotNotes& notes,
                   uint64_t tick);
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
  /// This tick's events so far; their ids are in `pending_ids_`.
  std::vector<LogicEvent> pending_;
  /// Each pending event's name.
  std::vector<std::string> pending_ids_;
  /// Actors removed this tick, which are not to be reported dead.
  std::vector<sim::EntityHandle> removed_;
};

}  // namespace eng::game
