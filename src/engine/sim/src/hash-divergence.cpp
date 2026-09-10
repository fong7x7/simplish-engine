#include <engine/sim/hash-divergence.h>

namespace eng::sim {

std::optional<HashDivergence> findDivergence(const TickHash& expected,
                                             const TickHash& actual) {
  if (expected.section_count != actual.section_count) {
    return HashDivergence{actual.tick, {}};
  }
  if (expected.combined == actual.combined) {
    return std::nullopt;
  }
  const auto want = expected.activeSections();
  const auto got = actual.activeSections();
  for (std::size_t i = 0; i < got.size(); ++i) {
    if (want[i].hash != got[i].hash) {
      return HashDivergence{actual.tick, got[i].name};
    }
  }
  // Every section agrees and only the combination differs: the combined
  // hash itself was corrupted, which no subsystem is responsible for.
  return HashDivergence{actual.tick, {}};
}

}  // namespace eng::sim
