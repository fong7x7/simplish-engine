#include <algorithm>
#include <game/player/player-system.h>
#include <game/world/logic-event-log.h>

namespace eng::game {

namespace {

  /// @p handle as the logic names an entity of pool @p kind.
  LogicTarget targetOf(LogicTargetKind kind, sim::EntityHandle handle) {
    return {kind, handle.index, handle.generation};
  }

}  // namespace

void LogicEventLog::note(const LogicEvent& event) {
  pending_.push_back({event.kind, event.target, event.at, {}});
  pending_ids_.emplace_back(event.id);
}

void LogicEventLog::noteRemoved(sim::EntityHandle handle) {
  removed_.push_back(handle);
}

void LogicEventLog::noteActors(const ActorPool& actors,
                               std::span<const std::string> actor_ids,
                               uint64_t tick) {
  for (uint32_t i = 0; i < actors.slots.size(); ++i) {
    const sim::EntityHandle handle = actors.slots.handleAt(i);
    const bool dead = actors.health[i] == 0;
    if ((dead && std::ranges::find(removed_, handle) != removed_.end()) ||
        (!dead && actors.damaged_tick[i] != tick)) {
      continue;
    }
    note({dead ? LogicEventKind::ACTOR_DIED : LogicEventKind::ACTOR_HURT,
          targetOf(LogicTargetKind::ACTOR, handle), actors.position[i],
          handle.index < actor_ids.size() ? actor_ids[handle.index] : ""});
  }
}

void LogicEventLog::notePlayers(const PlayerPool& players, uint64_t tick) {
  for (uint32_t i = 0; i < players.slots.size(); ++i) {
    const bool downed =
        players.downed[i] != 0 && players.downed_since[i] == tick;
    const bool hurt = playerIsUp(players, i) &&
                      players.hurt_until[i] == tick + PLAYER_HURT_GRACE_TICKS;
    if (downed || hurt) {
      note(
          {downed ? LogicEventKind::PLAYER_DOWNED : LogicEventKind::PLAYER_HURT,
           targetOf(LogicTargetKind::PLAYER, players.slots.handleAt(i)),
           players.position[i],
           {}});
    }
  }
}

void LogicEventLog::publish() {
  events_.swap(pending_);
  ids_.swap(pending_ids_);
  for (size_t i = 0; i < events_.size(); ++i) {
    events_[i].id = ids_[i];
  }
  pending_.clear();
  pending_ids_.clear();
  removed_.clear();
}

void LogicEventLog::hashInto(sim::StateHasher& hasher) const {
  for (const LogicEvent& event : events_) {
    hasher.add(event.kind);
    hasher.add(event.target.kind);
    hasher.add(event.target.index);
    hasher.add(event.target.generation);
    hasher.add(event.at);
  }
}

}  // namespace eng::game
