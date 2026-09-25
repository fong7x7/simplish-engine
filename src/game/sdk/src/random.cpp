#include <game/sdk/random.h>

namespace eng::game::sdk {

bool chance(GameLogicWorld& world, uint32_t permille) {
  return world.random(1000) < permille;
}

int32_t between(GameLogicWorld& world, int32_t low, int32_t high) {
  if (high <= low) {
    return low;
  }
  const auto span = static_cast<uint32_t>(static_cast<int64_t>(high) - low + 1);
  return static_cast<int32_t>(static_cast<int64_t>(low) + world.random(span));
}

}  // namespace eng::game::sdk
