#include "trace-ring.h"

#include <string>

namespace eng::net {

void keepTraced(const sim::TickHash& hash, std::deque<sim::TickHash>& ring) {
  ring.push_back(hash);
  if (ring.size() > NET_TRACE_TICKS) {
    ring.pop_front();
  }
}

NetTrace traceOf(uint16_t run, const std::deque<sim::TickHash>& ring) {
  NetTrace trace{run, {}, {ring.begin(), ring.end()}};
  if (!ring.empty()) {
    for (const sim::TickHashSection& section : ring.back().activeSections()) {
      trace.section_names.emplace_back(section.name);
    }
  }
  for (sim::TickHash& hash : trace.ticks) {
    for (sim::TickHashSection& section : hash.sections) {
      section.name = {};  // Names travel once, not per tick.
    }
  }
  return trace;
}

}  // namespace eng::net
