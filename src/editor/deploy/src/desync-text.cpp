#include "desync-text.h"

#include <engine/net/net-hash-report.h>

namespace eng::editor {

namespace {

  /// The section @p desync names, in words.
  std::string sectionName(const net::NetDesync& desync,
                          const std::deque<sim::TickHash>& checkpoints) {
    if (desync.section == net::NET_SECTION_COUNT_DIFFERS) {
      return "the number of sections (different builds?)";
    }
    for (const sim::TickHash& hash : checkpoints) {
      if (hash.tick == desync.tick && desync.section < hash.section_count &&
          !hash.sections[desync.section].name.empty()) {
        return "section " + std::string(hash.sections[desync.section].name);
      }
    }
    return "section " + std::to_string(desync.section);
  }

  /// Who @p slot is, in words.
  std::string who(uint8_t slot) {
    return slot == net::NET_SERVER_SLOT
               ? std::string("the server")
               : "player " + std::to_string(static_cast<int>(slot) + 1);
  }

}  // namespace

std::string describeDesync(const net::NetDesync& desync,
                           const std::deque<sim::TickHash>& checkpoints) {
  return "Desync at tick " + std::to_string(desync.tick) + " in " +
         sectionName(desync, checkpoints) + ", reported by " + who(desync.slot);
}

void keepCheckpoint(const sim::TickHash& hash,
                    std::deque<sim::TickHash>& checkpoints) {
  if (hash.tick % net::NET_HASH_INTERVAL != 0) {
    return;
  }
  checkpoints.push_back(hash);
  if (checkpoints.size() > DEPLOYED_CHECKPOINTS) {
    checkpoints.pop_front();
  }
}

}  // namespace eng::editor
