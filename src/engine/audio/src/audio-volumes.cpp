#include <algorithm>
#include <engine/audio/audio-volumes.h>

namespace eng::audio {

float audioVolumeGain(float volume) {
  const float clamped = std::clamp(volume, 0.0F, 1.0F);
  return clamped * clamped;
}

float busVolume(const AudioVolumes& volumes, AudioBus bus) {
  return volumes.buses[static_cast<size_t>(bus)];
}

void setBusVolume(AudioVolumes& volumes, AudioBus bus, float volume) {
  volumes.buses[static_cast<size_t>(bus)] = std::clamp(volume, 0.0F, 1.0F);
}

void applyAudioVolumes(AudioEngine& engine, const AudioVolumes& volumes) {
  const bool muted = volumes.muting == AudioMuting::MUTED;
  engine.setMasterGain(muted ? 0.0F : audioVolumeGain(volumes.master));
  for (uint8_t i = 0; i < AUDIO_BUS_COUNT; ++i) {
    const auto bus = static_cast<AudioBus>(i);
    engine.setBusGain(bus, audioVolumeGain(busVolume(volumes, bus)));
  }
}

}  // namespace eng::audio
