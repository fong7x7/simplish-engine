// No audio backend: a target without one, or a console build without its
// private overlay. The game runs silent; the engine still mixes if asked.

#include <engine/audio/audio-device.h>

namespace eng::audio {

AudioDevice::~AudioDevice() {
  close();
}

std::optional<std::string> AudioDevice::open(AudioEngine& /*engine*/) {
  return "this build has no audio backend";
}

void AudioDevice::close() {
  stream_ = nullptr;
}

}  // namespace eng::audio
