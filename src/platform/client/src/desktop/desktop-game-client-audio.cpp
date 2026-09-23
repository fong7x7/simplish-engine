// The sound half of DesktopGameClient: opening the platform's audio device
// on the client's engine. Which output exists is platform/audio's business;
// this owns the device's lifetime, and never lets a missing one stop the
// game.

#include "engine/client/desktop-game-client.h"

#include <engine/core/logger.h>

namespace eng::client {

void DesktopGameClient::openAudio() {
  if (const std::optional<std::string> error = audio_device_.open(audio_)) {
    LOG_WARN("client", "No sound: " + *error);
  }
}

void DesktopGameClient::openDevices() {
  openGamepads();
  openAudio();
}

}  // namespace eng::client
