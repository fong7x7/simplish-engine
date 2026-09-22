#include <algorithm>
#include <engine/input/gamepad-seats.h>

namespace eng::input {

void GamepadSeats::update(const GamepadSet& pads) {
  const std::vector<uint64_t> connected = pads.devices();
  for (std::optional<uint64_t>& seat : seats_) {
    if (seat && std::ranges::find(connected, *seat) == connected.end()) {
      seat.reset();
    }
  }
  for (const uint64_t device : connected) {
    if (seatOf(device) || !pads.wasTouched(device)) {
      continue;
    }
    auto* const free = std::ranges::find_if(
        seats_, [](const std::optional<uint64_t>& seat) { return !seat; });
    if (free != seats_.end()) {
      *free = device;
    }
  }
}

std::optional<uint64_t> GamepadSeats::device(uint8_t seat) const {
  return seat < seats_.size() ? seats_[seat] : std::nullopt;
}

std::optional<uint8_t> GamepadSeats::seatOf(uint64_t device) const {
  const auto* const found = std::ranges::find(seats_, std::optional{device});
  return found != seats_.end()
             ? std::optional{static_cast<uint8_t>(found - seats_.begin())}
             : std::nullopt;
}

bool GamepadSeats::occupied(uint8_t seat) const {
  return device(seat).has_value();
}

}  // namespace eng::input
