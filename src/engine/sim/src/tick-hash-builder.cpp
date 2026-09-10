#include <engine/core/assert.h>
#include <engine/sim/tick-hash-builder.h>

namespace eng::sim {

StateHasher& TickHashBuilder::section(std::string_view name) {
  // ENGINE_ASSERT expands to the do-while statement-macro idiom.
  // NOLINTNEXTLINE(cppcoreguidelines-avoid-do-while)
  ENGINE_ASSERT(count_ < MAX_TICK_HASH_SECTIONS,
                "a tick hash has more sections than MAX_TICK_HASH_SECTIONS");
  names_[count_] = name;
  return hashers_[count_++];
}

TickHash TickHashBuilder::finish(uint64_t tick) const {
  TickHash hash;
  hash.tick = tick;
  hash.section_count = count_;
  StateHasher combined;
  for (std::size_t i = 0; i < count_; ++i) {
    hash.sections[i] = TickHashSection{names_[i], hashers_[i].value()};
    combined.add(hash.sections[i].hash);
  }
  hash.combined = combined.value();
  return hash;
}

}  // namespace eng::sim
