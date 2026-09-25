#include <array>
#include <engine/render-water/water-splash.h>

namespace eng {

namespace {

  /// Droplets flung up and falling back, drawn as short streaks.
  constexpr FxBurst DROPLETS{
      .count = 16,
      .spread_degrees = 28.0f,
      .speed_min = 1.4f,
      .speed_max = 3.0f,
      .life_min = 0.35f,
      .life_max = 0.65f,
      .look = {.size_start = 0.035f,
               .size_end = 0.02f,
               .color_start = {0.62f, 0.7f, 0.75f, 0.85f},
               .color_end = {0.45f, 0.55f, 0.6f, 0.6f},
               .gravity = 9.0f,
               .drag = 0.4f,
               .stretch = 0.035f,
               .lighting = FxParticleLighting::LIT}};

  /// A lower ring of droplets thrown wide.
  constexpr FxBurst RING{.count = 12,
                         .spread_degrees = 75.0f,
                         .speed_min = 0.9f,
                         .speed_max = 1.8f,
                         .life_min = 0.25f,
                         .life_max = 0.45f,
                         .look = {.size_start = 0.03f,
                                  .size_end = 0.015f,
                                  .color_start = {0.6f, 0.68f, 0.72f, 0.8f},
                                  .color_end = {0.4f, 0.5f, 0.55f, 0.5f},
                                  .gravity = 8.0f,
                                  .stretch = 0.03f,
                                  .lighting = FxParticleLighting::LIT}};

  /// A wisp of mist hanging over it for a moment.
  constexpr FxBurst MIST{.count = 4,
                         .spread_degrees = 60.0f,
                         .speed_min = 0.2f,
                         .speed_max = 0.6f,
                         .life_min = 0.5f,
                         .life_max = 0.9f,
                         .look = {.size_start = 0.08f,
                                  .size_end = 0.26f,
                                  .color_start = {0.22f, 0.25f, 0.27f, 0.3f},
                                  .color_end = {0.0f, 0.0f, 0.0f, 0.0f},
                                  .gravity = -0.2f,
                                  .drag = 1.5f,
                                  .spin = 30.0f,
                                  .shape = FxParticleShape::PUFF,
                                  .lighting = FxParticleLighting::LIT}};

  /// The splash's bursts, in the order they are thrown.
  constexpr std::array<FxBurst, 3> BURSTS{DROPLETS, RING, MIST};

  /// The splash.
  constexpr FxEffect SPLASH{BURSTS, {}, {}};

}  // namespace

const FxEffect& waterSplashEffect() {
  return SPLASH;
}

}  // namespace eng
