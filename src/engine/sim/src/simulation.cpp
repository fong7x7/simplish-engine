#include <engine/sim/simulation.h>

namespace eng::sim {

Simulation::Simulation(SimulationSystems& systems, TickHashing hashing)
  : systems_(&systems), hashing_(hashing) {}

TickResult Simulation::step(const TickInput& input) {
  const TickContext context{next_tick_, input};
  runPhases(context);
  TickResult result{context.tick, std::nullopt};
  if (hashing_ == TickHashing::ON) {
    result.hash = hashTick(context.tick);
  }
  ++next_tick_;
  return result;
}

void Simulation::runPhases(const TickContext& context) {
  // Engine REQUIREMENTS §4.1, steps 2–8. This order is the determinism
  // contract: moving a line here changes what every replay simulates.
  systems_->playerControl(context);
  systems_->enemyAi(context);
  systems_->weaponFire(context);
  systems_->projectiles(context);
  systems_->damage(context);
  systems_->director(context);
  systems_->compaction(context);
}

TickHash Simulation::hashTick(uint64_t tick) const {
  TickHashBuilder builder;
  systems_->hashState(builder);
  return builder.finish(tick);
}

}  // namespace eng::sim
