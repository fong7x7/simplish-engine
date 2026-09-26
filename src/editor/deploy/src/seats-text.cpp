#include "seats-text.h"

#include <engine/net/net-desync.h>
#include <engine/sim/tick-input.h>
#include <vector>

namespace eng::editor {

std::string seatText(uint8_t slot) {
  return slot == net::NET_SERVER_SLOT
             ? std::string("the server")
             : "player " + std::to_string(static_cast<int>(slot) + 1);
}

std::string seatsText(uint8_t mask) {
  std::vector<std::string> numbers;
  for (uint8_t slot = 0; slot < sim::MAX_PLAYERS; ++slot) {
    if ((mask & (1U << slot)) != 0) {
      numbers.push_back(std::to_string(static_cast<int>(slot) + 1));
    }
  }
  if (numbers.empty()) {
    return {};
  }
  std::string text = numbers.size() == 1 ? "player " : "players ";
  for (std::size_t i = 0; i < numbers.size(); ++i) {
    const bool last = i + 1 == numbers.size();
    text += (i == 0 ? "" : (last ? " and " : ", ")) + numbers[i];
  }
  return text;
}

}  // namespace eng::editor
