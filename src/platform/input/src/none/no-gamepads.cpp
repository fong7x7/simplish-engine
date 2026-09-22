// The pad backend for a target with none compiled in: no pads, ever. A
// console build without its private overlay lands here, and plays with
// whatever else it has rather than failing to build.

#include <engine/input/gamepads.h>

namespace eng::input {

Gamepads::~Gamepads() {
  close();
}

std::optional<std::string> Gamepads::open() {
  opened_ = true;
  return std::nullopt;
}

void Gamepads::close() {
  opened_ = false;
}

bool Gamepads::rumble(const GamepadRumble& /*rumble*/) {
  return false;
}

bool Gamepads::rumble(uint64_t /*device*/, const GamepadRumble& /*rumble*/) {
  return false;
}

void Gamepads::poll() {
  pads_.update({});
}

}  // namespace eng::input
