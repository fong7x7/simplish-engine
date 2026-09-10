#include <engine/core/assert.h>
#include <engine/sim/replay-verification.h>

namespace eng::sim {

ReplayVerification verifyReplay(const Replay& replay, Simulation& simulation) {
  // ENGINE_ASSERT is a do-while macro that negates its whole condition.
  // NOLINTNEXTLINE(cppcoreguidelines-avoid-do-while,readability-simplify-boolean-expr)
  ENGINE_ASSERT(simulation.nextTick() == 0 &&
                    simulation.hashing() == TickHashing::ON,
                "verifyReplay needs a hashing simulation at tick 0");
  ReplayVerification verification;
  std::size_t next_checkpoint = 0;
  for (const TickInput& input : replay.inputs) {
    const TickResult result = simulation.step(input);
    ++verification.ticks_run;
    if (!result.hash || next_checkpoint == replay.checkpoints.size() ||
        replay.checkpoints[next_checkpoint].tick != result.tick) {
      continue;
    }
    verification.divergence =
        findDivergence(replay.checkpoints[next_checkpoint++], *result.hash);
    if (verification.divergence) {
      break;
    }
  }
  return verification;
}

}  // namespace eng::sim
