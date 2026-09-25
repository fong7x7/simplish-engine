#include <game/world/logic-combatant.h>
#include <game/world/logic-event-log.h>

namespace eng::game {


void LogicEventLog::note(const LogicEvent& event) {
  pending_.push_back(event);
  pending_.back().id = {};
  pending_ids_.emplace_back(event.id);
}

void LogicEventLog::publish() {
  events_.swap(pending_);
  ids_.swap(pending_ids_);
  for (size_t i = 0; i < events_.size(); ++i) {
    events_[i].id = ids_[i];
  }
  pending_.clear();
  pending_ids_.clear();
}

void LogicEventLog::hashInto(sim::StateHasher& hasher) const {
  for (const LogicEvent& event : events_) {
    hasher.add(event.kind);
    hasher.add(event.target.kind);
    hasher.add(event.target.index);
    hasher.add(event.target.generation);
    hasher.add(event.at);
    hashCombatantRef(combatantOf(event.by), hasher);
    hasher.add(event.amount);
    hasher.add(event.cause);
  }
}

}  // namespace eng::game
