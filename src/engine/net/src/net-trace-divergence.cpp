#include <algorithm>
#include <engine/net/net-desync.h>
#include <engine/net/net-trace-divergence.h>

namespace eng::net {

namespace {

  /// @p trace's hash of @p tick, if it has one.
  const sim::TickHash* hashAt(const NetTrace& trace, uint64_t tick) {
    for (const sim::TickHash& hash : trace.ticks) {
      if (hash.tick == tick) {
        return &hash;
      }
    }
    return nullptr;
  }

  /// The first section @p a and @p b differ on, or
  /// `NET_SECTION_COUNT_DIFFERS`.
  uint8_t firstSection(const sim::TickHash& a, const sim::TickHash& b) {
    if (a.section_count != b.section_count) {
      return NET_SECTION_COUNT_DIFFERS;
    }
    for (std::size_t i = 0; i < a.section_count; ++i) {
      if (a.sections[i].hash != b.sections[i].hash) {
        return static_cast<uint8_t>(i);
      }
    }
    return NET_SECTION_COUNT_DIFFERS;
  }

  /// The name of section @p section in whichever of @p traces names it.
  std::string nameOf(std::span<const NetPeerTrace> traces, uint8_t section) {
    for (const NetPeerTrace& peer : traces) {
      if (section < peer.trace.section_names.size()) {
        return peer.trace.section_names[section];
      }
    }
    return {};
  }

  /// Where the traces disagree on @p tick, if they do: each is compared
  /// with the first that hashed it.
  std::optional<NetTraceDivergence>
  divergenceAt(std::span<const NetPeerTrace> traces, uint64_t tick) {
    const NetPeerTrace* reference = nullptr;
    const sim::TickHash* expected = nullptr;
    for (const NetPeerTrace& peer : traces) {
      const sim::TickHash* hash = hashAt(peer.trace, tick);
      if (hash != nullptr && expected == nullptr) {
        reference = &peer;
        expected = hash;
      } else if (hash != nullptr && hash->combined != expected->combined) {
        const uint8_t section = firstSection(*expected, *hash);
        return NetTraceDivergence{tick, section, nameOf(traces, section),
                                  reference->slot, peer.slot};
      }
    }
    return std::nullopt;
  }

}  // namespace

std::optional<NetTraceDivergence>
findTraceDivergence(std::span<const NetPeerTrace> traces) {
  uint64_t earliest = UINT64_MAX;
  uint64_t latest = 0;
  for (const NetPeerTrace& peer : traces) {
    if (!peer.trace.ticks.empty()) {
      earliest = std::min(earliest, peer.trace.ticks.front().tick);
      latest = std::max(latest, peer.trace.ticks.back().tick);
    }
  }
  for (uint64_t tick = earliest; tick <= latest && earliest != UINT64_MAX;
       ++tick) {
    if (auto found = divergenceAt(traces, tick)) {
      return found;
    }
  }
  return std::nullopt;
}

}  // namespace eng::net
