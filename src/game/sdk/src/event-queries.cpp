#include <algorithm>
#include <game/sdk/event-queries.h>

namespace eng::game::sdk {

std::vector<LogicEvent> findEvents(const GameLogicWorld& world,
                                   const EventFilter& filter) {
  std::vector<LogicEvent> found;
  for (const LogicEvent& event : world.events()) {
    if (matches(filter, event)) {
      found.push_back(event);
    }
  }
  return found;
}

uint32_t countEvents(const GameLogicWorld& world, const EventFilter& filter) {
  return static_cast<uint32_t>(
      std::ranges::count_if(world.events(), [&filter](const LogicEvent& e) {
        return matches(filter, e);
      }));
}

bool heard(const GameLogicWorld& world, const EventFilter& filter) {
  return std::ranges::any_of(world.events(), [&filter](const LogicEvent& e) {
    return matches(filter, e);
  });
}

uint32_t totalAmount(const GameLogicWorld& world, const EventFilter& filter) {
  uint32_t total = 0;
  for (const LogicEvent& event : world.events()) {
    total += matches(filter, event) ? event.amount : 0U;
  }
  return total;
}

}  // namespace eng::game::sdk
