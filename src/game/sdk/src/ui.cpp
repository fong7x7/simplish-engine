#include <game/sdk/ui.h>
#include <string>

namespace eng::game::sdk {

void setUiNumber(GameLogicWorld& world, std::string_view key, int64_t value) {
  world.setUiValue(key, std::to_string(value));
}

bool chose(const LogicEvent& event, std::string_view action) {
  return event.kind == LogicEventKind::UI_ACTION && event.id == action;
}

}  // namespace eng::game::sdk
