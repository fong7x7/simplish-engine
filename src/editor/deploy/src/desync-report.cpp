#include "desync-report.h"

#include "seats-text.h"

#include <algorithm>
#include <cstdio>
#include <sstream>

namespace eng::editor {

namespace {

  /// @p value as sixteen hex digits.
  std::string hex(uint64_t value) {
    std::array<char, 17> text{};
    (void)std::snprintf(text.data(), text.size(), "%016llx",
                        static_cast<unsigned long long>(value));
    return text.data();
  }

  /// @p trace's hash of @p tick, if it has one.
  const sim::TickHash* hashAt(const net::NetTrace& trace, uint64_t tick) {
    const auto found =
        std::ranges::find_if(trace.ticks, [tick](const sim::TickHash& hash) {
          return hash.tick == tick;
        });
    return found == trace.ticks.end() ? nullptr : &*found;
  }

  /// Which ticks each peer's trace covers.
  void writeCoverage(std::span<const net::NetPeerTrace> traces,
                     std::ostream& out) {
    out << "\nTraces:\n";
    for (const net::NetPeerTrace& peer : traces) {
      const auto& ticks = peer.trace.ticks;
      out << "  " << seatText(peer.slot) << ": ";
      if (ticks.empty()) {
        out << "no ticks\n";
        continue;
      }
      out << "ticks " << ticks.front().tick << "-" << ticks.back().tick << '\n';
    }
  }

  /// Every peer's combined hash of each tick around @p centre.
  void writeTicks(std::span<const net::NetPeerTrace> traces, uint64_t centre,
                  std::ostream& out) {
    out << "\nCombined hash by tick:\n";
    const uint64_t from = centre - std::min(centre, DESYNC_REPORT_SPAN);
    for (uint64_t tick = from; tick <= centre + DESYNC_REPORT_SPAN; ++tick) {
      out << (tick == centre ? "> " : "  ") << tick;
      for (const net::NetPeerTrace& peer : traces) {
        const sim::TickHash* hash = hashAt(peer.trace, tick);
        out << "  " << seatText(peer.slot) << " "
            << (hash != nullptr ? hex(hash->combined) : "-");
      }
      out << '\n';
    }
  }

  /// The name of section @p index, from the first trace that names it.
  std::string sectionName(std::span<const net::NetPeerTrace> traces,
                          std::size_t index) {
    for (const net::NetPeerTrace& peer : traces) {
      if (index < peer.trace.section_names.size()) {
        return peer.trace.section_names[index];
      }
    }
    return "#" + std::to_string(index);
  }

  /// Every peer's hash of each section on @p tick.
  void writeSections(std::span<const net::NetPeerTrace> traces, uint64_t tick,
                     std::ostream& out) {
    out << "\nSections on tick " << tick << ":\n";
    for (std::size_t index = 0; index < sim::MAX_TICK_HASH_SECTIONS; ++index) {
      std::ostringstream row;
      bool any = false;
      for (const net::NetPeerTrace& peer : traces) {
        const sim::TickHash* hash = hashAt(peer.trace, tick);
        const bool has = hash != nullptr && index < hash->section_count;
        any = any || has;
        row << "  " << seatText(peer.slot) << " "
            << (has ? hex(hash->sections[index].hash) : "-");
      }
      if (any) {
        out << "  " << sectionName(traces, index) << row.str() << '\n';
      }
    }
  }

}  // namespace

std::filesystem::path desyncReportPath(const std::filesystem::path& dir,
                                       const std::string& level,
                                       uint64_t tick) {
  return (dir.empty() ? std::filesystem::current_path() : dir) /
         ("simplish-desync-" + level + "-tick" + std::to_string(tick) + ".txt");
}

std::string
divergenceText(const std::optional<net::NetTraceDivergence>& divergence) {
  if (!divergence) {
    return "the traces agree wherever they overlap: it diverged earlier "
           "than they reach";
  }
  const std::string section = divergence->section_name.empty()
                                  ? "the number of sections"
                                  : "section " + divergence->section_name;
  return "first diverged at tick " + std::to_string(divergence->tick) + " in " +
         section + ", between " + seatText(divergence->first_slot) + " and " +
         seatText(divergence->other_slot);
}

std::string desyncReport(const std::string& level, const net::NetDesync& desync,
                         std::span<const net::NetPeerTrace> traces) {
  const auto divergence = net::findTraceDivergence(traces);
  std::ostringstream out;
  out << "Simplish desync report\n\nLevel " << level << ", run " << desync.run
      << ". Caught at tick " << desync.tick << ", when "
      << seatText(desync.slot)
      << " disagreed with the first hash reported.\nThe run "
      << divergenceText(divergence) << ".\n";
  writeCoverage(traces, out);
  const uint64_t centre = divergence ? divergence->tick : desync.tick;
  writeTicks(traces, centre, out);
  writeSections(traces, centre, out);
  return out.str();
}

}  // namespace eng::editor
