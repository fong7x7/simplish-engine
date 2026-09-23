#include <engine/audio/audio-clip.h>

namespace eng::audio {

uint32_t clipFrames(const AudioClip& clip) {
  if (clip.channels == 0) {
    return 0;
  }
  return static_cast<uint32_t>(clip.samples.size() / clip.channels);
}

float clipSeconds(const AudioClip& clip) {
  if (clip.sample_rate == 0) {
    return 0.0F;
  }
  return static_cast<float>(clipFrames(clip)) /
         static_cast<float>(clip.sample_rate);
}

}  // namespace eng::audio
