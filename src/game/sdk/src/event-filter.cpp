#include <game/sdk/event-filter.h>

namespace eng::game::sdk {

bool matches(const EventFilter& filter, const LogicEvent& event) {
  return (!filter.kind || event.kind == *filter.kind) &&
         (!filter.target || event.target == *filter.target) &&
         (filter.id.empty() || event.id == filter.id) &&
         event.id.starts_with(filter.id_prefix) &&
         (!filter.by || event.by == filter.by) &&
         (!filter.cause || event.cause == *filter.cause) &&
         (filter.state.empty() || event.state == filter.state);
}

}  // namespace eng::game::sdk
