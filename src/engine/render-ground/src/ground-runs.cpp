#include <engine/render-ground/ground-runs.h>

namespace eng {

namespace {

  /// Add one more cell of @p terrain to the end of @p runs.
  void extend(std::vector<GroundRun>& runs, uint8_t terrain) {
    if (!runs.empty() && runs.back().terrain == terrain) {
      ++runs.back().length;
    } else {
      runs.push_back({terrain, 1});
    }
  }

  /// How many cells @p runs covers, stopping once it passes @p cap so a
  /// hostile length cannot overflow the sum.
  uint64_t countCells(std::span<const GroundRun> runs, uint64_t cap) {
    uint64_t total = 0;
    for (const GroundRun& run : runs) {
      total += run.length;
      if (total > cap) {
        break;
      }
    }
    return total;
  }

}  // namespace

std::vector<GroundRun> encodeGroundRuns(const GroundGrid& grid,
                                        GroundRect bounds) {
  std::vector<GroundRun> runs;
  for (int32_t row = 0; row < bounds.height; ++row) {
    for (int32_t column = 0; column < bounds.width; ++column) {
      extend(runs, grid.at({bounds.x + column, bounds.y + row}));
    }
  }
  return runs;
}

std::optional<GroundGrid> decodeGroundRuns(GroundRect bounds,
                                           std::span<const GroundRun> runs) {
  if (bounds.width < 0 || bounds.height < 0) {
    return std::nullopt;
  }
  const uint64_t expected = static_cast<uint64_t>(bounds.width) *
                            static_cast<uint64_t>(bounds.height);
  const uint64_t span = static_cast<uint64_t>(GROUND_COORDINATE_LIMIT) * 2;
  // Refused before a cell is allocated: a rectangle past the limit is one
  // `fromCells` would refuse anyway, after the damage.
  if (expected > span * span || countCells(runs, expected) != expected) {
    return std::nullopt;
  }
  std::vector<uint8_t> cells;
  cells.reserve(expected);
  for (const GroundRun& run : runs) {
    cells.insert(cells.end(), run.length, run.terrain);
  }
  return GroundGrid::fromCells(bounds, std::move(cells));
}

}  // namespace eng
