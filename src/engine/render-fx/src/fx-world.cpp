#include <engine/render-fx/fx-world.h>

namespace eng {

FxWorld::FxWorld(uint64_t seed) : rng(seed, FX_RNG_STREAM) {}

void playFxEffect(FxWorld& world, const FxEffect& effect, const FxEmit& emit) {
  for (const FxBurst& burst : effect.bursts) {
    (void)emitFxBurst(world.particles, burst, emit, world.rng);
  }
  FxFlash flash = effect.flash;
  flash.range *= emit.scale;
  emitFxFlash(world.lights, flash, emit.at);
}

void stepFxWorld(FxWorld& world, float seconds) {
  stepFxParticles(world.particles, seconds);
  stepFxLights(world.lights, seconds);
}

void clearFxWorld(FxWorld& world) {
  clearFxParticles(world.particles);
  clearFxLights(world.lights);
}

}  // namespace eng
