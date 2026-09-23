#include <algorithm>
#include <cmath>
#include <engine/audio/audio-spatial.h>
#include <numbers>

namespace eng::audio {

float distanceGain(const AudioListener& listener, Vec3 at) {
  const float distance = std::hypot(at.x - listener.at.x, at.y - listener.at.y);
  if (distance <= listener.full_tiles) {
    return 1.0F;
  }
  if (distance >= listener.silent_tiles) {
    return 0.0F;
  }
  const float fade = listener.silent_tiles - listener.full_tiles;
  const float left = 1.0F - (distance - listener.full_tiles) / fade;
  return left * left;
}

float pan(const AudioListener& listener, Vec3 at) {
  const float side = (at.x - listener.at.x) * listener.right.x +
                     (at.y - listener.at.y) * listener.right.y;
  const float reach = std::max(listener.full_tiles, 0.5F) * 2.0F;
  return std::clamp(side / reach, -1.0F, 1.0F) * AUDIO_MAX_PAN;
}

StereoGain panGain(float pan) {
  const float angle =
      (std::clamp(pan, -1.0F, 1.0F) + 1.0F) * std::numbers::pi_v<float> / 4.0F;
  return {.left = std::cos(angle) * std::numbers::sqrt2_v<float>,
          .right = std::sin(angle) * std::numbers::sqrt2_v<float>};
}

StereoGain spatialGain(const AudioListener& listener, Vec3 at) {
  const float gain = distanceGain(listener, at);
  const StereoGain panned = panGain(pan(listener, at));
  return {.left = panned.left * gain, .right = panned.right * gain};
}

}  // namespace eng::audio
